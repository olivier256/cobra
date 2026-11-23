/* =========================================================================
   Lecture des BMP 8 bits indexés, pour les outils (voir bmpread.h).
   ========================================================================= */

#include "bmpread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bmp_fail(const char *fmt, const char *arg) {
    fprintf(stderr, "%s : ", bmp_tool_name);
    fprintf(stderr, fmt, arg);
    fprintf(stderr, "\n");
    exit(1);
}

/* ---- Lecture BMP --------------------------------------------------------- */

static uint32_t le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

void bmp_load(const char *path, BmpImage *img) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        bmp_fail("impossible d'ouvrir %s", path);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size);
    if (data == NULL || fread(data, 1, (size_t)size, f) != (size_t)size)
        bmp_fail("lecture impossible : %s", path);
    fclose(f);

    if (size < 54 || data[0] != 'B' || data[1] != 'M')
        bmp_fail("%s n'est pas un fichier BMP", path);

    uint32_t pixelOffset = le32(data + 10);
    uint32_t infoSize = le32(data + 14);
    int32_t width = (int32_t)le32(data + 18);
    int32_t height = (int32_t)le32(data + 22);
    uint16_t bpp = le16(data + 28);
    uint32_t compression = le32(data + 30);
    uint32_t colorsUsed = le32(data + 46);

    if (infoSize < 40)
        bmp_fail("%s : en-tête BMP trop ancien (OS/2)", path);
    if (bpp != 8)
        bmp_fail("%s : il faut une image 8 bits indexée (256 couleurs)", path);
    if (compression != 0)
        bmp_fail("%s : la compression RLE n'est pas prise en charge", path);

    int topDown = height < 0;
    if (topDown)
        height = -height;
    if (width <= 0 || height <= 0)
        bmp_fail("%s : dimensions invalides", path);

    img->width = width;
    img->height = height;
    img->colors = colorsUsed ? (int)colorsUsed : 256;
    if (img->colors > 256)
        bmp_fail("%s : palette de plus de 256 couleurs", path);

    memset(img->rgb, 0, sizeof img->rgb);
    const uint8_t *pal = data + 14 + infoSize;
    if (pal + img->colors * 4 > data + size)
        bmp_fail("%s : palette tronquée", path);
    for (int i = 0; i < img->colors; i++) {
        img->rgb[i][0] = pal[i * 4 + 2]; /* BMP : B, G, R, réservé */
        img->rgb[i][1] = pal[i * 4 + 1];
        img->rgb[i][2] = pal[i * 4 + 0];
    }

    /* Chaque ligne est complétée à un multiple de 4 octets */
    size_t stride = ((size_t)width + 3) & ~(size_t)3;
    if (pixelOffset + stride * (size_t)height > (size_t)size)
        bmp_fail("%s : pixels tronqués", path);

    img->pixels = malloc((size_t)width * height);
    if (img->pixels == NULL)
        bmp_fail("mémoire insuffisante pour %s", path);
    for (int y = 0; y < height; y++) {
        int srcY = topDown ? y : height - 1 - y;
        memcpy(img->pixels + (size_t)y * width,
               data + pixelOffset + (size_t)srcY * stride, (size_t)width);
    }
    free(data);
}
