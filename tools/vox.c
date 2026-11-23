/* =========================================================================
   Lecture du .vox de MagicaVoxel et de CUBES.txt (repris de dds32conv).
   ========================================================================= */

#include "conv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iso.h"

static uint32_t rd32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

void vox_load(const char *path, Voxels *v) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        bmp_fail("impossible d'ouvrir %s", path);
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)size);
    if (data == NULL || fread(data, 1, (size_t)size, f) != (size_t)size)
        bmp_fail("lecture impossible : %s", path);
    fclose(f);
    if (size < 20 || memcmp(data, "VOX ", 4) != 0 ||
        memcmp(data + 8, "MAIN", 4) != 0)
        bmp_fail("%s n'est pas un fichier MagicaVoxel", path);

    /* Les blocs fils de MAIN : on prend le premier SIZE et le premier XYZI */
    long p = 20 + (long)rd32(data + 12);
    int haveSize = 0, haveVoxels = 0;
    v->voxels = NULL;
    while (p + 12 <= size && !haveVoxels) {
        const uint8_t *id = data + p;
        const uint32_t n = rd32(data + p + 4), children = rd32(data + p + 8);
        const uint8_t *content = data + p + 12;
        if (p + 12 + (long)n > size)
            bmp_fail("%s : bloc tronqué", path);
        if (memcmp(id, "SIZE", 4) == 0 && !haveSize) {
            if (n < 12)
                bmp_fail("%s : bloc SIZE tronqué", path);
            v->W = (int)rd32(content);
            v->D = (int)rd32(content + 4);
            v->H = (int)rd32(content + 8);
            if (v->W < 1 || v->D < 1 || v->H < 1 || v->H > 16 || v->W > 255 ||
                v->D > 255)
                bmp_fail("%s : dimensions refusées (hauteur : 16 au plus)",
                         path);
            v->voxels = calloc((size_t)v->W * v->D * v->H, 1);
            haveSize = 1;
        } else if (memcmp(id, "XYZI", 4) == 0 && haveSize) {
            const uint32_t count = rd32(content);
            if (4 + 4 * (long)count > (long)n)
                bmp_fail("%s : bloc XYZI tronqué", path);
            for (uint32_t c = 0; c < count; c++) {
                const uint8_t *q = content + 4 + 4 * c;
                if (q[0] < v->W && q[1] < v->D && q[2] < v->H)
                    v->voxels[(q[2] * v->D + q[1]) * v->W + q[0]] = q[3];
            }
            haveVoxels = 1;
        }
        p += 12 + (long)n + (long)children;
    }
    if (!haveVoxels)
        bmp_fail("%s : pas de voxels", path);
    free(data);
}

/* ---- CUBES.txt ---------------------------------------------------------- */

#define MAX_IMAGES 64
#define MAX_LINE 1024

/* Face (ISO_FACE_TOP, ...) du pixel (x, y) d'un sprite de cube, -1 dehors */
static int cube_face(int x, int y) {
    for (int slot = 0; slot < 6; slot++) {
        const int dc = iso_cube_slots[slot].dc, dt = iso_cube_slots[slot].dt;
        const int r = y - ISO_ROW_PITCH * dt;
        if (r < 0 || r >= ISO_TRI_ROWS || x / ISO_TRI_W != dc)
            continue;
        const int start = iso_row_start(dc, dt, r);
        const int lx = x % ISO_TRI_W;
        if (lx >= start && lx < start + iso_row_width(r))
            return iso_cube_slots[slot].face;
    }
    return -1;
}

void cubes_load(const char *path, const char *imageDir, Cubes *c) {
    static const char *const faceNames[ISO_FACES] = {"top", "left", "right"};
    static char names[MAX_IMAGES][64];
    static BmpImage images[MAX_IMAGES];
    int imageCount = 0;

    memset(c->present, 0, sizeof c->present);
    FILE *f = fopen(path, "r");
    if (f == NULL)
        bmp_fail("impossible d'ouvrir %s", path);
    char line[MAX_LINE];
    int lineNo = 0;
    while (fgets(line, sizeof line, f) != NULL) {
        lineNo++;
        char *hash = strchr(line, '#');
        if (hash != NULL)
            *hash = '\0';
        int type;
        char refs[ISO_FACES][128];
        const int n = sscanf(line, "%d %127s %127s %127s", &type, refs[0],
                             refs[1], refs[2]);
        if (n <= 0)
            continue;
        if (n != 4 || type < 1 || type > 255) {
            fprintf(stderr,
                    "cobraconv : %s, ligne %d : 'type dessus gauche droite' "
                    "attendu\n",
                    path, lineNo);
            exit(1);
        }
        const BmpImage *img[ISO_FACES];
        for (int face = 0; face < ISO_FACES; face++) {
            char *dot = strrchr(refs[face], '.');
            if (dot == NULL || strcmp(dot + 1, faceNames[face]) != 0) {
                fprintf(stderr,
                        "cobraconv : %s, ligne %d : '%s' : il faut NOM.%s\n",
                        path, lineNo, refs[face], faceNames[face]);
                exit(1);
            }
            *dot = '\0';
            int found = -1;
            for (int m = 0; m < imageCount; m++)
                if (strcmp(names[m], refs[face]) == 0)
                    found = m;
            if (found < 0) {
                if (imageCount == MAX_IMAGES)
                    bmp_fail("trop d'images (%s)", refs[face]);
                found = imageCount++;
                snprintf(names[found], sizeof names[found], "%s", refs[face]);
                char file[2 * MAX_LINE];
                snprintf(file, sizeof file, "%s/%s.bmp", imageDir, refs[face]);
                bmp_load(file, &images[found]);
                if (images[found].width != CUBE_SIZE ||
                    images[found].height != CUBE_SIZE)
                    bmp_fail("%s : il faut une image de 32 x 32", file);
                if (found == 0)
                    memcpy(c->rgb, images[0].rgb, sizeof c->rgb);
            }
            img[face] = &images[found];
        }
        c->present[type] = 1;
        for (int y = 0; y < CUBE_SIZE; y++)
            for (int x = 0; x < CUBE_SIZE; x++) {
                const int face = cube_face(x, y);
                c->sprite[type][y * CUBE_SIZE + x] =
                    face < 0 ? 0 : img[face]->pixels[y * CUBE_SIZE + x];
            }
    }
    fclose(f);
}
