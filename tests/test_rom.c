/* =========================================================================
   test_rom : la ROM (rom/cobra.bin) sur le banc sans affichage.

   1. Elle démarre : aucune demande que le modèle du VDP ne connaît pas,
      aucun accès inconnu, le compteur d'images avance.
   2. Le décor affiché (sans les sprites) est, pixel pour pixel, l'image
      attendue du convertisseur (build/data/attendu.bmp), à la position de
      la caméra : au départ et tout au long d'une promenade à la manette.
   3. L'ombre : au départ, le héros est posé sur un sol dégagé ; les
      pixels sous l'opérateur d'ombre sont assombris de moitié, exactement.
   4. Les recouvrements : le héros est posé à des positions de la zone
      (une sur trois) ; à chaque pixel de son sprite, il doit être visible
      si et seulement si aucun voxel n'est devant lui (le même prédicat que
      le convertisseur, voir test_scene). Aucune erreur permise hors des
      positions en conflit ; pour celles-ci, on compte.
   5. Les cycles de chaque image (V_INT et boucle principale), au plus
      grand, doivent tenir dans une image.

   À lancer depuis la racine du dépôt, après make data.
   ========================================================================= */

#include "conv.h"
#include "md.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *bmp_tool_name = "test_rom";

static int failures = 0;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            fprintf(stderr, "ÉCHEC : " __VA_ARGS__);                           \
            fprintf(stderr, "\n");                                             \
        }                                                                      \
    } while (0)

/* BMP 24 bits (celui qu'écrit cobraconv), en niveaux 0 à 7 */
static uint8_t *load_bmp24(const char *path, int *w, int *h) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        bmp_fail("impossible d'ouvrir %s", path);
    uint8_t hdr[54];
    if (fread(hdr, 1, 54, f) != 54 || hdr[0] != 'B' || hdr[28] != 24)
        bmp_fail("%s : BMP 24 bits attendu", path);
    *w = hdr[18] | (hdr[19] << 8);
    *h = hdr[22] | (hdr[23] << 8);
    const int stride = (3 * *w + 3) & ~3;
    uint8_t *rgb = malloc((size_t)*w * *h * 3), *row = malloc((size_t)stride);
    for (int y = *h - 1; y >= 0; y--) {
        if (fread(row, 1, (size_t)stride, f) != (size_t)stride)
            bmp_fail("%s : tronqué", path);
        for (int x = 0; x < *w; x++)
            for (int c = 0; c < 3; c++)
                rgb[3 * ((size_t)y * *w + x) + c] =
                    (uint8_t)(row[3 * x + 2 - c] / 36);
    }
    free(row);
    fclose(f);
    return rgb;
}

static uint8_t *expected;
static int expW, expH;

/* Le décor affiché contre l'image attendue ; renvoie les pixels faux */
static long check_decor(Md *m) {
    static uint8_t rgb[VDP_W * VDP_H * 3];
    vdp_render(&m->vdp, rgb, 0, NULL);
    const int cx = -vdp_scroll_x(&m->vdp), cy = vdp_scroll_y(&m->vdp);
    long bad = 0;
    for (int y = 0; y < VDP_H; y++)
        for (int x = 0; x < VDP_W; x++) {
            const int ex = x + cx, ey = y + cy;
            const uint8_t *e = (ex >= 0 && ey >= 0 && ex < expW && ey < expH)
                                   ? expected + 3 * (ey * expW + ex)
                                   : expected; /* le fond */
            const uint8_t *g = rgb + 3 * (y * VDP_W + x);
            for (int c = 0; c < 3; c++)
                bad += g[c] != 2 * e[c];
        }
    return bad;
}

static void place(Md *m, int u, int v, int w) {
    md_set_ram16(m, m->heroAddr + 0, (uint16_t)u);
    md_set_ram16(m, m->heroAddr + 2, (uint16_t)v);
    md_set_ram16(m, m->heroAddr + 4, (uint16_t)w);
    md_set_ram16(m, m->heroAddr + 6, (uint16_t)(w * 16));
    md_set_ram16(m, m->heroAddr + 8, 0);
}

int main(void) {
    static Md md;
    CHECK(md_open(&md, "rom/cobra.bin") == 0, "rom/cobra.bin illisible");
    if (failures)
        return 1;
    expected = load_bmp24("build/data/attendu.bmp", &expW, &expH);

    /* 1. Démarrage : le chargement de la VRAM, puis des images */
    int boot = 0;
    while (boot < 60 && md_ram32(&md, md.frameAddr) == 0) {
        md_frame(&md, 0);
        boot++;
    }
    for (int f = 0; f < 10; f++)
        md_frame(&md, 0);
    CHECK(!md.vdp.error, "VDP : %s", md.vdp.errorText);
    CHECK(!md.badAccess, "accès inconnu en $%06X", md.badAddress);
    const uint32_t frames0 = md_ram32(&md, md.frameAddr);
    CHECK(boot < 60 && frames0 >= 9, "la ROM n'affiche pas d'images (%u)",
          frames0);
    printf("démarrage : première image après %d images de chargement\n",
           boot);

    /* 2. Le décor, au départ */
    long bad = check_decor(&md);
    CHECK(bad == 0, "décor au départ : %ld composantes fausses", bad);

    /* 3. L'ombre au départ : opérateur sous le héros, sur le plan B */
    {
        static uint8_t with[VDP_W * VDP_H * 3], without[VDP_W * VDP_H * 3];
        static uint8_t source[VDP_W * VDP_H];
        vdp_render(&md.vdp, with, 1, source);
        vdp_render(&md.vdp, without, 0, NULL);
        int shadowed = 0, hero = 0, other = 0;
        for (int p = 0; p < VDP_W * VDP_H; p++) {
            if (source[p] == 1) {
                hero++;
                continue;
            }
            if (memcmp(with + 3 * p, without + 3 * p, 3) == 0)
                continue;
            int half = 1;
            for (int c = 0; c < 3; c++)
                half &= with[3 * p + c] == without[3 * p + c] / 2;
            shadowed += half;
            other += !half;
        }
        CHECK(other == 0, "ombre : %d pixels changés autrement que de moitié",
              other);
        CHECK(shadowed >= 60 && shadowed <= 100,
              "ombre : %d pixels assombris (100 sous l'opérateur, moins ceux "
              "cachés par le héros)",
              shadowed);
        CHECK(hero >= 150, "héros : %d pixels visibles au départ", hero);
        printf("départ : %d pixels du héros, %d pixels assombris par l'ombre\n",
               hero, shadowed);
    }

    /* 2 (suite) et 5 : une promenade */
    static const struct {
        int frames;
        uint16_t pad;
    } walk[] = {
        {40, PAD_RIGHT},          {30, PAD_DOWN},
        {1, PAD_DOWN | PAD_C},    {50, PAD_DOWN},
        {60, PAD_RIGHT},          {1, PAD_RIGHT | PAD_A},
        {40, PAD_RIGHT},          {60, PAD_DOWN | PAD_RIGHT},
        {80, PAD_LEFT},           {1, PAD_B},
        {30, 0},                  {60, PAD_UP},
        {60, PAD_LEFT | PAD_UP},  {40, PAD_DOWN | PAD_LEFT},
    };
    uint32_t maxBusy = 0, minBusy = 0xFFFFFFFF;
    long decorBad = 0;
    int checks = 0;
    for (size_t s = 0; s < sizeof walk / sizeof walk[0]; s++) {
        for (int f = 0; f < walk[s].frames; f++) {
            const uint32_t before = md.measures;
            md_frame(&md, walk[s].pad);
            if (md.measures != before) {
                if (md.busy > maxBusy)
                    maxBusy = md.busy;
                if (md.busy < minBusy)
                    minBusy = md.busy;
            }
            if (f % 10 == 9) {
                decorBad += check_decor(&md);
                checks++;
            }
        }
    }
    CHECK(decorBad == 0, "décor pendant la promenade : %ld composantes fausses",
          decorBad);
    CHECK(!md.vdp.error && !md.badAccess, "erreur pendant la promenade");
    CHECK(maxBusy < MD_FRAME_CYCLES, "une image déborde : %u cycles", maxBusy);
    const uint32_t h = md.heroAddr;
    printf("promenade : %d vérifications du décor ; héros arrivé en u = %d, "
           "v = %d, w = %d\n",
           checks, (int16_t)md_ram16(&md, h), (int16_t)md_ram16(&md, h + 2),
           (int16_t)md_ram16(&md, h + 4));
    printf("cycles par image (V_INT et boucle principale) : %u à %u, soit "
           "%.2f %% à %.2f %% d'une image NTSC\n",
           minBusy, maxBusy, 100.0 * minBusy / MD_FRAME_CYCLES,
           100.0 * maxBusy / MD_FRAME_CYCLES);

    /* 4. Les recouvrements, position par position */
    static Voxels vox;
    static Cubes cubes;
    vox_load("demo/zone/demo.vox", &vox);
    cubes_load("demo/zone/cubes.txt", "demo/assets", &cubes);
    Scene scene;
    scene_render(&scene, &vox, &cubes);
    const HeroShape *hs = &HERO_DEFAULT;
    const int n = positions_list(&vox, hs, NULL, NULL, 0);
    Pos *pos = malloc(sizeof *pos * (size_t)(n + 1));
    uint8_t *walkable = malloc((size_t)n + 1), *cls = malloc((size_t)n + 1);
    uint8_t *inA = malloc((size_t)scene.width * scene.height);
    positions_list(&vox, hs, pos, walkable, n);
    planes_assign(&scene, hs, pos, n, inA);
    planes_classify(&scene, hs, pos, n, inA, cls);

    int tested = 0, conflicts = 0, conflictsWrong = 0, wrongPositions = 0;
    long wrongPixels = 0, conflictPixels = 0;
    for (int q = 0; q < n; q += 3) {
        if (!walkable[q])
            continue;
        place(&md, pos[q].u, pos[q].v, pos[q].w);
        md_frame(&md, 0);
        md_frame(&md, 0);
        CHECK(md_ram16(&md, h) == pos[q].u && md_ram16(&md, h + 4) == pos[q].w,
              "le héros n'est pas resté en (%d, %d, %d)", pos[q].u, pos[q].v,
              pos[q].w);
        static uint8_t rgb[VDP_W * VDP_H * 3], source[VDP_W * VDP_H];
        vdp_render(&md.vdp, rgb, 1, source);
        const int cx = -vdp_scroll_x(&md.vdp), cy = vdp_scroll_y(&md.vdp);
        /* Le sprite du héros : entrée 0 de la table des sprites */
        const uint32_t sat = (uint32_t)(md.vdp.reg[5] & 0x7E) << 9;
        const int sy = (((md.vdp.vram[sat] << 8) | md.vdp.vram[sat + 1]) & 0x3FF) - 128;
        const int sx = (((md.vdp.vram[sat + 6] << 8) | md.vdp.vram[sat + 7]) & 0x1FF) - 128;
        const int tile = ((md.vdp.vram[sat + 4] << 8) | md.vdp.vram[sat + 5]) & 0x7FF;
        long wrong = 0;
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 16; x++) {
                const int X = sx + x, Y = sy + y;
                if (X < 0 || Y < 0 || X >= VDP_W || Y >= VDP_H)
                    continue;
                const uint8_t b = md.vdp.vram[(tile + (x >> 3) * 4 + (y >> 3)) * 32 +
                                              (y & 7) * 4 + ((x & 7) >> 1)];
                if (((x & 1) ? b & 15 : b >> 4) == 0)
                    continue; /* pixel transparent du sprite */
                const int ix = X + cx, iy = Y + cy;
                const int32_t o = (ix >= 0 && iy >= 0 && ix < scene.width &&
                                   iy < scene.height)
                                      ? scene.owner[iy * scene.width + ix]
                                      : -1;
                const int hidden = o >= 0 && voxel_in_front(&scene, o, hs, pos[q]);
                const int shown = source[Y * VDP_W + X] == 1;
                wrong += hidden == shown;
            }
        tested++;
        if (cls[q] & 2) {
            conflicts++;
            conflictPixels += wrong;
            conflictsWrong += wrong != 0;
        } else {
            wrongPixels += wrong;
            wrongPositions += wrong != 0;
        }
    }
    CHECK(wrongPositions == 0,
          "recouvrements : %d positions (hors conflits) fausses, %ld pixels",
          wrongPositions, wrongPixels);
    printf("recouvrements : %d positions essayées, justes hors conflits ; "
           "%d en conflit, dont %d visiblement fausses (%ld pixels)\n",
           tested, conflicts, conflictsWrong, conflictPixels);

    free(inA);
    free(cls);
    free(walkable);
    free(pos);
    scene_free(&scene);
    free(vox.voxels);
    free(expected);
    md_close(&md);
    printf("test_rom : %s\n", failures ? "ÉCHEC" : "OK");
    return failures != 0;
}
