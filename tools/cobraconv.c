/* =========================================================================
   cobraconv : du monde de voxels aux données de la ROM (voir conv.h).

   Usage : cobraconv ZONE.vox CUBES.txt DOSSIER_IMAGES HEROS.bmp SORTIE

   Écrit dans le dossier SORTIE :
   - tiles.bin     les tiles du décor, 32 octets chacun (le tile 0 est vide)
   - plane_a.bin   table du plan A, 64 x 64 mots (gros-boutiste)
   - plane_b.bin   table du plan B, idem
   - cram.bin      les quatre lignes de palette, 64 mots
   - sprites.bin   le héros (2 x 4 tiles, colonne par colonne) puis son
                   ombre (2 x 1 tiles, couleur 15 de la ligne 3)
   - voxels.bin    les voxels, W x D x H octets (k, puis j, puis i)
   - prio.bin      la priorité du héros à chaque position (voir plus bas)
   - zone.inc      les constantes pour l'assembleur
   - attendu.bmp   l'image des deux plans telle que le VDP doit l'afficher
   - plans.bmp     la même, plan A en jaune, positions du héros marquées
   - mesures.txt   les mesures

   prio.bin : un octet par position (u = 4 m + 2, v = 4 n + 2, w = 16 k),
   à l'adresse (k x MV + n) x MU + m, avec MU = 4 W, MV = 4 D :
   bit 0 : la priorité du sprite (1 = haute), bit 1 : conflit (des pixels
   du plan A devant le héros et d'autres derrière ; la priorité retenue est
   celle qui en fait le moins de faux) ; $FF : position impossible.
   ========================================================================= */

#include "conv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *bmp_tool_name = "cobraconv";

#define MAX_PATH 1024
#define VRAM_TILES 1472  /* $0000-$B7FF : tout ce qui n'est pas tables */
#define SPRITE_TILES 10  /* héros et ombre */
#define BEAM_TILES 80    /* réserve du rayon delta */

static FILE *open_out(const char *dir, const char *name, const char *mode) {
    char path[2 * MAX_PATH];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, mode);
    if (f == NULL)
        bmp_fail("impossible d'écrire %s", path);
    return f;
}

static void put16(FILE *f, uint16_t w) {
    fputc(w >> 8, f);
    fputc(w & 255, f);
}

static void write_bmp24(const char *dir, const char *name, const uint8_t *rgb,
                        int w, int h) {
    FILE *f = open_out(dir, name, "wb");
    const int stride = (3 * w + 3) & ~3;
    const uint32_t size = 54 + (uint32_t)stride * h;
    uint8_t hdr[54] = {'B', 'M'};
    const uint32_t fields[] = {size, 0, 54, 40, (uint32_t)w, (uint32_t)h};
    for (int i = 0; i < 6; i++)
        for (int b = 0; b < 4; b++)
            hdr[2 + 4 * i + b] = (uint8_t)(fields[i] >> (8 * b));
    hdr[26] = 1;
    hdr[28] = 24;
    fwrite(hdr, 1, sizeof hdr, f);
    uint8_t *row = calloc((size_t)stride, 1);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++)
            for (int c = 0; c < 3; c++)
                row[3 * x + c] = rgb[3 * ((size_t)y * w + x) + 2 - c];
        fwrite(row, 1, (size_t)stride, f);
    }
    free(row);
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "usage : cobraconv ZONE.vox CUBES.txt DOSSIER_IMAGES "
                        "HEROS.bmp SORTIE\n");
        return 2;
    }
    const char *out = argv[5];
    static Voxels vox;
    static Cubes cubes;
    vox_load(argv[1], &vox);
    cubes_load(argv[2], argv[3], &cubes);
    Scene scene;
    scene_render(&scene, &vox, &cubes);
    const int npix = scene.width * scene.height;

    MdPalette pal;
    palette_build(cubes.rgb, &pal);

    /* Positions du héros, plans, priorités */
    const HeroShape *hero = &HERO_DEFAULT;
    const int maxPos = positions_list(&vox, hero, NULL, NULL, 0);
    Pos *pos = malloc(sizeof *pos * (size_t)(maxPos + 1));
    uint8_t *walk = malloc((size_t)maxPos + 1);
    const int npos = positions_list(&vox, hero, pos, walk, maxPos);
    uint8_t *inA = malloc((size_t)npix);
    planes_assign(&scene, hero, pos, npos, inA);
    uint8_t *cls = malloc((size_t)npos + 1);
    planes_classify(&scene, hero, pos, npos, inA, cls);

    int walkCount = 0, byClass[4] = {0, 0, 0, 0}, decorPixels = 0, aPixels = 0;
    for (int q = 0; q < npos; q++)
        if (walk[q]) {
            walkCount++;
            byClass[cls[q]]++;
        }
    for (int p = 0; p < npix; p++) {
        decorPixels += scene.owner[p] >= 0;
        aPixels += inA[p];
    }

    /* Tiles et plans */
    static MdTiles tiles;
    tiles_build(&scene, inA, &pal, &tiles);

    FILE *f = open_out(out, "tiles.bin", "wb");
    fwrite(tiles.tiles, 32, (size_t)tiles.tileCount, f);
    fclose(f);
    f = open_out(out, "plane_a.bin", "wb");
    for (int n = 0; n < PLANE_W * PLANE_H; n++)
        put16(f, tiles.planeA[n]);
    fclose(f);
    f = open_out(out, "plane_b.bin", "wb");
    for (int n = 0; n < PLANE_W * PLANE_H; n++)
        put16(f, tiles.planeB[n]);
    fclose(f);
    f = open_out(out, "cram.bin", "wb");
    for (int line = 0; line < PALETTE_LINES; line++)
        for (int n = 0; n < 16; n++) {
            const uint8_t *c = pal.rgb[line][n];
            put16(f, (uint16_t)((c[2] << 9) | (c[1] << 5) | (c[0] << 1)));
        }
    fclose(f);

    /* Sprites : le héros (pieds en bas au milieu), puis l'ombre */
    BmpImage heroImg;
    bmp_load(argv[4], &heroImg);
    if (heroImg.width != hero->spriteW || heroImg.height != hero->spriteH)
        bmp_fail("%s : il faut une image de 16 x 32", argv[4]);
    f = open_out(out, "sprites.bin", "wb");
    for (int tx = 0; tx < 2; tx++)
        for (int ty = 0; ty < 4; ty++) {
            uint8_t px[64], packed[32];
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    const uint8_t c =
                        heroImg.pixels[(ty * 8 + y) * 16 + tx * 8 + x];
                    if (c != 0 && (c < 48 || c > 63))
                        bmp_fail("%s : couleurs 48 à 63 seulement (ligne 2)",
                                 argv[4]);
                    px[y * 8 + x] = c ? (uint8_t)(c - 48) : 0;
                }
            tile_pack(px, packed);
            fwrite(packed, 1, 32, f);
        }
    int shadowPixels = 0;
    for (int tx = 0; tx < 2; tx++) {
        uint8_t px[64], packed[32];
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                const double dx = (tx * 8 + x - 7.5) / 8.0,
                             dy = (y - 3.5) / 4.0;
                px[y * 8 + x] = dx * dx + dy * dy <= 1.0 ? 15 : 0;
                shadowPixels += px[y * 8 + x] != 0;
            }
        tile_pack(px, packed);
        fwrite(packed, 1, 32, f);
    }
    fclose(f);

    f = open_out(out, "voxels.bin", "wb");
    fwrite(vox.voxels, 1, (size_t)vox.W * vox.D * vox.H, f);
    fclose(f);

    const int MU = 4 * vox.W, MV = 4 * vox.D;
    uint8_t *prio = malloc((size_t)MU * MV * vox.H);
    memset(prio, PRIO_NONE, (size_t)MU * MV * vox.H);
    for (int q = 0; q < npos; q++)
        prio[((pos[q].w >> 4) * MV + (pos[q].v >> 2)) * MU + (pos[q].u >> 2)] =
            cls[q];
    f = open_out(out, "prio.bin", "wb");
    fwrite(prio, 1, (size_t)MU * MV * vox.H, f);
    fclose(f);

    f = open_out(out, "zone.inc", "w");
    fprintf(f,
            "; Généré par cobraconv depuis %s : ne pas modifier.\n"
            "ZONE_W\t\tEQU\t%d\nZONE_D\t\tEQU\t%d\nZONE_H\t\tEQU\t%d\n"
            "ORIGIN_X\tEQU\t%d\nORIGIN_Y\tEQU\t%d\n"
            "IMG_W\t\tEQU\t%d\nIMG_H\t\tEQU\t%d\n"
            "TILE_COUNT\tEQU\t%d\nPRIO_MU\t\tEQU\t%d\nPRIO_MV\t\tEQU\t%d\n",
            argv[1], vox.W, vox.D, vox.H, scene.originX, scene.originY,
            scene.width, scene.height, tiles.tileCount, MU, MV);
    fclose(f);

    /* Images */
    uint8_t *rgb = malloc((size_t)npix * 3), *img = malloc((size_t)npix * 3);
    tiles_render(&tiles, &pal, scene.width, scene.height, rgb);
    for (int p = 0; p < 3 * npix; p++)
        img[p] = (uint8_t)(rgb[p] * 36);
    write_bmp24(out, "attendu.bmp", img, scene.width, scene.height);
    for (int p = 0; p < npix; p++)
        if (inA[p]) {
            img[3 * p] = (uint8_t)((img[3 * p] + 255) / 2);
            img[3 * p + 1] = (uint8_t)((img[3 * p + 1] + 200) / 2);
            img[3 * p + 2] = (uint8_t)(img[3 * p + 2] / 2);
        }
    static const uint8_t mark[4][3] = {{40, 200, 40}, {40, 220, 255},
                                       {255, 30, 30}, {255, 30, 30}};
    for (int q = 0; q < npos; q++) {
        if (!walk[q])
            continue;
        int x0, y0, x1, y1;
        hero_rect(&scene, hero, pos[q], &x0, &y0, &x1, &y1);
        const int x = x0 + hero->spriteW / 2, y = y0 + hero->footRow;
        if (x >= 0 && y >= 0 && x < scene.width && y < scene.height)
            memcpy(img + 3 * ((size_t)y * scene.width + x), mark[cls[q]], 3);
    }
    write_bmp24(out, "plans.bmp", img, scene.width, scene.height);

    /* Mesures */
    const int decorBudget = VRAM_TILES - SPRITE_TILES - BEAM_TILES;
    char report[4096];
    snprintf(report, sizeof report,
             "zone : %d x %d x %d, image %d x %d\n"
             "positions du héros : %d (dont %d posées)\n"
             "  priorité basse %d (%.1f %%), haute %d (%.1f %%), conflit %d "
             "(%.1f %% ; basse %d, haute %d)\n"
             "plan A : %d pixels de décor sur %d (%.1f %%)\n"
             "tiles : %d avec retournements, %d sans (tile vide compris) ; "
             "budget du décor %d (marge %d)\n"
             "entrées non vides : plan A %d, plan B %d\n"
             "tiles qui mêlaient les deux lignes de palette : %d, pixels "
             "approchés : %d\n"
             "ombre : %d pixels\n",
             vox.W, vox.D, vox.H, scene.width, scene.height, npos, walkCount,
             byClass[0], 100.0 * byClass[0] / walkCount, byClass[1],
             100.0 * byClass[1] / walkCount, byClass[2] + byClass[3],
             100.0 * (byClass[2] + byClass[3]) / walkCount, byClass[2],
             byClass[3], aPixels, decorPixels,
             100.0 * aPixels / decorPixels, tiles.tileCount, tiles.tilesNoFlip,
             decorBudget, decorBudget - tiles.tileCount, tiles.usedA,
             tiles.usedB, tiles.mixedTiles, tiles.approxPixels, shadowPixels);
    f = open_out(out, "mesures.txt", "w");
    fputs(report, f);
    fclose(f);
    fputs(report, stderr);
    if (tiles.tileCount > decorBudget)
        bmp_fail("trop de tiles pour la VRAM%s", "");

    free(rgb);
    free(img);
    free(prio);
    free(cls);
    free(inA);
    free(walk);
    free(pos);
    tiles_free(&tiles);
    scene_free(&scene);
    free(vox.voxels);
    free(heroImg.pixels);
    return 0;
}
