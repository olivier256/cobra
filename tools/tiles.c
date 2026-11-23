/* =========================================================================
   Palettes, tiles et tables des plans de la Mega Drive (voir conv.h).
   ========================================================================= */

#include "conv.h"

#include <stdlib.h>
#include <string.h>

/* ---- Palette ------------------------------------------------------------- */

static void md_color(const uint8_t rgb[3], uint8_t out[3]) {
    for (int c = 0; c < 3; c++) {
        if (rgb[c] % 36 != 0 || rgb[c] > 252)
            bmp_fail("une couleur de la palette n'est pas une couleur de la "
                     "Mega Drive (composantes multiples de 36)%s",
                     "");
        out[c] = (uint8_t)(rgb[c] / 36);
    }
}

void palette_build(const uint8_t rgb[256][3], MdPalette *pal) {
    memset(pal, 0, sizeof *pal);
    for (int line = 0; line < 2; line++) {
        const int first = line ? 32 : 16;
        md_color(rgb[1], pal->rgb[line][0]);
        for (int n = 0; n < 15; n++)
            md_color(rgb[first + n], pal->rgb[line][1 + n]);
    }
    for (int n = 1; n < 16; n++)
        md_color(rgb[48 + n], pal->rgb[2][n]);
    /* Ligne 3 : les ennemis ; 14 et 15 sont les opérateurs de surbrillance
       et d'ombre (leur couleur ne s'affiche jamais) */
    for (int c = 0; c < 3; c++)
        pal->rgb[3][14][c] = 7;
}

int palette_line(uint8_t index) {
    if (index >= 16 && index < 16 + DECOR_COLORS)
        return 0;
    if (index >= 32 && index < 32 + DECOR_COLORS)
        return 1;
    return -1;
}

int palette_color(uint8_t index) {
    const int line = palette_line(index);
    return line < 0 ? 0 : index - (line ? 31 : 15);
}

/* ---- Tiles ---------------------------------------------------------------- */

void tile_pack(const uint8_t px[64], uint8_t out[32]) {
    for (int n = 0; n < 32; n++)
        out[n] = (uint8_t)((px[2 * n] << 4) | (px[2 * n + 1] & 15));
}

void tile_unpack(const uint8_t in[32], uint8_t px[64]) {
    for (int n = 0; n < 32; n++) {
        px[2 * n] = in[n] >> 4;
        px[2 * n + 1] = in[n] & 15;
    }
}

static void orient(const uint8_t in[64], int o, uint8_t out[64]) {
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            const int sx = (o & 1) ? 7 - x : x, sy = (o & 2) ? 7 - y : y;
            out[y * 8 + x] = in[sy * 8 + sx];
        }
}

/* Table de hachage des tiles déjà rangés */
typedef struct {
    int size;
    int *slot; /* numéro du tile + 1, 0 = libre */
} Table;

static uint32_t hash(const uint8_t px[64]) {
    uint32_t h = 2166136261u;
    for (int n = 0; n < 64; n++)
        h = (h ^ px[n]) * 16777619u;
    return h;
}

static int table_find(const Table *t, uint8_t (*tiles)[32], const uint8_t
                      px[64]) {
    uint8_t packed[32];
    tile_pack(px, packed);
    for (uint32_t h = hash(px) % (uint32_t)t->size;; h = (h + 1) % (uint32_t)t->size) {
        if (t->slot[h] == 0)
            return -(int)h - 1;
        if (memcmp(tiles[t->slot[h] - 1], packed, 32) == 0)
            return t->slot[h] - 1;
    }
}

void tiles_build(const Scene *s, const uint8_t *inA, const MdPalette *pal,
                 MdTiles *t) {
    const int tw = s->width / 8, th = s->height / 8;
    if (tw > PLANE_W || th > PLANE_H || s->width % 8 || s->height % 8)
        bmp_fail("l'image de la zone ne tient pas dans un plan de 64 x 64 "
                 "tiles%s",
                 "");
    memset(t, 0, sizeof *t);
    const int maxTiles = 2 * tw * th + 1;
    t->tiles = calloc((size_t)maxTiles, 32);
    Table table = {4 * maxTiles + 1, NULL}, noFlip = {4 * maxTiles + 1, NULL};
    table.slot = calloc((size_t)table.size, sizeof(int));
    noFlip.slot = calloc((size_t)noFlip.size, sizeof(int));
    uint8_t (*noFlipTiles)[32] = calloc((size_t)maxTiles, 32);
    int noFlipCount = 1;
    t->tileCount = 1; /* tile 0 : vide */
    for (int n = 0; n < PLANE_W * PLANE_H; n++)
        t->planeA[n] = 0x8000; /* prioritaire, même vide */

    for (int plane = 0; plane < 2; plane++)
        for (int ty = 0; ty < th; ty++)
            for (int tx = 0; tx < tw; tx++) {
                uint8_t idx[64];
                int count[2] = {0, 0};
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++) {
                        const int o = (ty * 8 + y) * s->width + tx * 8 + x;
                        const int mine = plane == 0 ? inA[o] : !inA[o];
                        idx[y * 8 + x] = mine ? s->color[o] : 0;
                        const int line = palette_line(idx[y * 8 + x]);
                        if (line >= 0)
                            count[line]++;
                    }
                if (count[0] + count[1] == 0)
                    continue;
                /* La ligne de palette : celle qui change le moins l'image */
                int bestLine = count[1] > count[0];
                if (count[0] && count[1]) {
                    long err[2] = {0, 0};
                    for (int line = 0; line < 2; line++)
                        for (int p = 0; p < 64; p++) {
                            const int l = palette_line(idx[p]);
                            if (l < 0 || l == line)
                                continue;
                            const uint8_t *c =
                                pal->rgb[l][palette_color(idx[p])];
                            long best = -1;
                            for (int n = 1; n <= DECOR_COLORS; n++) {
                                const uint8_t *d = pal->rgb[line][n];
                                const long e = (c[0] - d[0]) * (c[0] - d[0]) +
                                               (c[1] - d[1]) * (c[1] - d[1]) +
                                               (c[2] - d[2]) * (c[2] - d[2]);
                                if (best < 0 || e < best)
                                    best = e;
                            }
                            err[line] += best;
                        }
                    bestLine = err[1] < err[0] ||
                               (err[1] == err[0] && count[1] > count[0]);
                    t->mixedTiles++;
                }
                uint8_t px[64];
                for (int p = 0; p < 64; p++) {
                    const int l = palette_line(idx[p]);
                    if (l < 0) {
                        px[p] = 0;
                    } else if (l == bestLine) {
                        px[p] = (uint8_t)palette_color(idx[p]);
                    } else {
                        const uint8_t *c = pal->rgb[l][palette_color(idx[p])];
                        long best = -1;
                        for (int n = 1; n <= DECOR_COLORS; n++) {
                            const uint8_t *d = pal->rgb[bestLine][n];
                            const long e = (c[0] - d[0]) * (c[0] - d[0]) +
                                           (c[1] - d[1]) * (c[1] - d[1]) +
                                           (c[2] - d[2]) * (c[2] - d[2]);
                            if (best < 0 || e < best) {
                                best = e;
                                px[p] = (uint8_t)n;
                            }
                        }
                        if (best > 0)
                            t->approxPixels++;
                    }
                }
                /* Sans retournement (pour la mesure) */
                const int nf = table_find(&noFlip, noFlipTiles, px);
                if (nf < 0) {
                    tile_pack(px, noFlipTiles[noFlipCount]);
                    noFlip.slot[-nf - 1] = ++noFlipCount;
                }
                /* Avec retournements */
                int index = -1, flags = 0, freeSlot = -1;
                for (int o = 0; o < 4 && index < 0; o++) {
                    uint8_t op[64];
                    orient(px, o, op);
                    const int f = table_find(&table, t->tiles, op);
                    if (f >= 0) {
                        index = f;
                        flags = o;
                    } else if (o == 0) {
                        freeSlot = -f - 1;
                    }
                }
                if (index < 0) {
                    index = t->tileCount++;
                    tile_pack(px, t->tiles[index]);
                    table.slot[freeSlot] = index + 1;
                }
                if (index >= 2048)
                    bmp_fail("plus de 2048 tiles%s", "");
                const uint16_t entry =
                    (uint16_t)((plane == 0 ? 0x8000 : 0) | (bestLine << 13) |
                               ((flags & 2) << 11) | ((flags & 1) << 11) |
                               index);
                if (plane == 0) {
                    t->planeA[ty * PLANE_W + tx] = entry;
                    t->usedA++;
                } else {
                    t->planeB[ty * PLANE_W + tx] = entry;
                    t->usedB++;
                }
            }
    t->tilesNoFlip = noFlipCount - 1 + 1; /* le tile vide compris */
    free(table.slot);
    free(noFlip.slot);
    free(noFlipTiles);
}

void tiles_free(MdTiles *t) {
    free(t->tiles);
    t->tiles = NULL;
}

static int plane_pixel(const MdTiles *t, uint16_t entry, int x, int y) {
    uint8_t px[64];
    tile_unpack(t->tiles[entry & 0x7ff], px);
    const int sx = (entry & 0x800) ? 7 - (x & 7) : (x & 7);
    const int sy = (entry & 0x1000) ? 7 - (y & 7) : (y & 7);
    return px[sy * 8 + sx];
}

void tiles_render(const MdTiles *t, const MdPalette *pal, int width,
                  int height, uint8_t *rgb) {
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            const int n = (y / 8) * PLANE_W + x / 8;
            const uint16_t a = t->planeA[n], b = t->planeB[n];
            const int pa = plane_pixel(t, a, x, y), pb = plane_pixel(t, b, x, y);
            const uint8_t *c = pal->rgb[0][0];
            if (pa)
                c = pal->rgb[(a >> 13) & 3][pa];
            else if (pb)
                c = pal->rgb[(b >> 13) & 3][pb];
            memcpy(rgb + 3 * ((size_t)y * width + x), c, 3);
        }
}
