/* =========================================================================
   cobrasim : la ROM de Cobra sur le banc sans affichage.

   Usage : cobrasim ROM [SCRIPT] [IMAGE.bmp]

   SCRIPT : des lignes « N BOUTONS » : la manette dans cet état pendant N
   images. BOUTONS : U D L R A B C S, ou « - » pour rien. Exemple :
       60 -
       40 R
       1 RC
       40 R
   Sans script : 300 images sans toucher à rien.

   Affiche les cycles de chaque image (V_INT et boucle principale) : le
   plus petit, le plus grand, la moyenne, en part des 127 856 cycles d'une
   image NTSC ; écrit la dernière image en BMP si demandé.
   ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "md.h"

static uint16_t buttons(const char *s) {
    uint16_t p = 0;
    for (; *s; s++)
        switch (*s) {
        case 'U': p |= PAD_UP; break;
        case 'D': p |= PAD_DOWN; break;
        case 'L': p |= PAD_LEFT; break;
        case 'R': p |= PAD_RIGHT; break;
        case 'A': p |= PAD_A; break;
        case 'B': p |= PAD_B; break;
        case 'C': p |= PAD_C; break;
        case 'S': p |= PAD_START; break;
        default: break;
        }
    return p;
}

static void write_bmp(const char *path, const uint8_t *rgb) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "cobrasim : impossible d'écrire %s\n", path);
        exit(1);
    }
    const int w = VDP_W, h = VDP_H, stride = 3 * w;
    uint8_t hdr[54] = {'B', 'M'};
    const uint32_t fields[] = {54 + (uint32_t)stride * h, 0, 54, 40,
                               (uint32_t)w, (uint32_t)h};
    for (int i = 0; i < 6; i++)
        for (int b = 0; b < 4; b++)
            hdr[2 + 4 * i + b] = (uint8_t)(fields[i] >> (8 * b));
    hdr[26] = 1;
    hdr[28] = 24;
    fwrite(hdr, 1, sizeof hdr, f);
    for (int y = h - 1; y >= 0; y--)
        for (int x = 0; x < w; x++)
            for (int c = 2; c >= 0; c--)
                fputc(rgb[3 * (y * w + x) + c] * 18, f);
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage : cobrasim ROM [SCRIPT] [IMAGE.bmp]\n");
        return 2;
    }
    static Md md;
    if (md_open(&md, argv[1]) != 0) {
        fprintf(stderr, "cobrasim : %s : pas une ROM de Cobra\n", argv[1]);
        return 1;
    }
    FILE *script = NULL;
    if (argc >= 3 && strcmp(argv[2], "-") != 0) {
        script = fopen(argv[2], "r");
        if (script == NULL) {
            fprintf(stderr, "cobrasim : impossible d'ouvrir %s\n", argv[2]);
            return 1;
        }
    }
    uint32_t minBusy = 0xFFFFFFFF, maxBusy = 0;
    uint64_t sumBusy = 0;
    int frames = 0, measured = 0;
    char line[256];
    int left = script ? 0 : 300;
    uint16_t pad = 0;
    for (;;) {
        while (left == 0) {
            if (script == NULL || fgets(line, sizeof line, script) == NULL)
                goto done;
            char b[64] = "-";
            if (sscanf(line, "%d %63s", &left, b) < 1 || line[0] == '#')
                left = 0;
            pad = buttons(b);
        }
        const uint32_t before = md.measures;
        md_frame(&md, pad);
        frames++;
        left--;
        if (md.measures != before) {
            measured++;
            sumBusy += md.busy;
            if (md.busy < minBusy)
                minBusy = md.busy;
            if (md.busy > maxBusy)
                maxBusy = md.busy;
        }
        if (md.vdp.error || md.badAccess)
            break;
    }
done:
    if (md.vdp.error)
        fprintf(stderr, "cobrasim : VDP : %s\n", md.vdp.errorText);
    if (md.badAccess)
        fprintf(stderr, "cobrasim : accès inconnu en $%06X\n", md.badAddress);
    const uint32_t h = md.heroAddr;
    printf("%d images, %u affichées par la ROM ; héros en u = %d, v = %d, "
           "w = %d\n",
           frames, md_ram32(&md, md.frameAddr), (int16_t)md_ram16(&md, h),
           (int16_t)md_ram16(&md, h + 2), (int16_t)md_ram16(&md, h + 4));
    if (measured)
        printf("cycles par image : %u à %u, moyenne %llu (%.1f %% à %.1f %% "
               "d'une image NTSC)\n",
               minBusy, maxBusy, (unsigned long long)(sumBusy / measured),
               100.0 * minBusy / MD_FRAME_CYCLES,
               100.0 * maxBusy / MD_FRAME_CYCLES);
    if (argc == 4) {
        static uint8_t rgb[VDP_W * VDP_H * 3];
        vdp_render(&md.vdp, rgb, 1, NULL);
        write_bmp(argv[3], rgb);
    }
    if (script)
        fclose(script);
    md_close(&md);
    return md.vdp.error || md.badAccess;
}
