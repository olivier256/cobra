/* =========================================================================
   test_scene : le convertisseur contre des références naïves.

   1. L'image (ordre du peintre) contre un lancer de rayons, pixel par
      pixel : la zone de démo et 100 zones au hasard. Seuls les pixels
      posés exactement sur une arête (deux voxels à la même profondeur)
      peuvent différer.
   2. « Ce voxel est-il devant le héros ? » (boîtes séparées) contre le
      rayon : partout où le rayon touche la boîte du héros ; de même pour
      la boîte (plate) de son ombre.
   3. Le plan A contre sa définition, pixel par pixel : un pixel y est si
      et seulement s'il cache le héros ou son ombre à l'une des positions ;
      le classement des positions contre sa définition.
   4. Les tiles : l'image décodée (tables des plans, tiles, palette) est
      celle de la scène, sauf les pixels comptés comme approchés.

   À lancer depuis la racine du dépôt (images de demo/assets).
   ========================================================================= */

#include "conv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *bmp_tool_name = "test_scene";

static int failures = 0;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            fprintf(stderr, "ÉCHEC : " __VA_ARGS__);                           \
            fprintf(stderr, "\n");                                             \
        }                                                                      \
    } while (0)

static uint32_t rng = 12345;
static int rnd(int n) {
    rng = rng * 1103515245u + 12345u;
    return (int)((rng >> 8) % (uint32_t)n);
}

/* 1. Peintre contre rayons ; renvoie le nombre de pixels comparés */
static long check_raycast(const Scene *s, const char *name) {
    long bad = 0, ties = 0, n = 0;
    for (int y = 0; y < s->height; y++)
        for (int x = 0; x < s->width; x++) {
            const int32_t o = s->owner[y * s->width + x];
            const int32_t r = scene_raycast(s, x, y);
            n++;
            if (o == r)
                continue;
            double ho, hr;
            if (o >= 0 && r >= 0 && scene_ray_hits(s, x, y, o, &ho) &&
                scene_ray_hits(s, x, y, r, &hr) && ho == hr)
                ties++;
            else
                bad++;
        }
    CHECK(bad == 0, "%s : %ld pixels différents du lancer de rayons", name,
          bad);
    (void)ties;
    return n;
}

/* 2. et 3. */
static int inside(const Scene *s, const HeroShape *h, Pos p, int x, int y) {
    int x0, y0, x1, y1;
    hero_rect(s, h, p, &x0, &y0, &x1, &y1);
    return x >= x0 && x < x1 && y >= y0 && y < y1;
}

/* step : une ligne et une position sur 'step' pour les références lentes
   (la zone de démo) */
static void check_planes(const Scene *s, const char *name, long *compared,
                         int step) {
    const Sprites *sp = &SPRITES_DEFAULT;
    const HeroShape *h = &sp->hero;
    const int n = positions_list(s->vox, h, NULL, NULL, 0);
    Pos *pos = malloc(sizeof *pos * (size_t)(n + 1));
    positions_list(s->vox, h, pos, NULL, n);

    /* 2. Le prédicat contre le rayon (une position sur 3), pour le héros
       et pour son ombre */
    long disagree = 0;
    for (int q = 0; q < n; q += 3)
        for (int which = 0; which < 2; which++) {
            const HeroShape *b = which ? &sp->shadow : h;
            int x0, y0, x1, y1;
            hero_rect(s, b, pos[q], &x0, &y0, &x1, &y1);
            for (int y = y0 - 4; y < y1 + 4; y++)
                for (int x = x0; x < x1; x++) {
                    if (x < 0 || y < 0 || x >= s->width || y >= s->height)
                        continue;
                    const int32_t o = s->owner[y * s->width + x];
                    const int ray = ray_voxel_in_front(s, x, y, b, pos[q]);
                    if (o < 0 || ray < 0)
                        continue;
                    (*compared)++;
                    if (ray != voxel_in_front(s, o, b, pos[q]))
                        disagree++;
                }
        }
    CHECK(disagree == 0, "%s : %ld pixels où le prédicat contredit le rayon",
          name, disagree);

    /* 3. Plan A contre sa définition (référence : pixel par pixel) */
    uint8_t *inA = malloc((size_t)s->width * s->height);
    planes_assign(s, sp, pos, n, inA);
    long wrong = 0;
    for (int y = 0; y < s->height; y += step)
        for (int x = 0; x < s->width; x++) {
            const int32_t o = s->owner[y * s->width + x];
            int want = 0;
            for (int q = 0; q < n && !want && o >= 0; q++) {
                if (inside(s, h, pos[q], x, y))
                    want = voxel_in_front(s, o, h, pos[q]);
                else if (inside(s, &sp->shadow, pos[q], x, y))
                    want = voxel_in_front(s, o, &sp->shadow, pos[q]);
            }
            wrong += want != inA[y * s->width + x];
        }
    CHECK(wrong == 0, "%s : %ld pixels mal rangés entre les plans", name,
          wrong);

    /* Classement : référence directe, position par position */
    uint8_t *cls = malloc((size_t)n + 1);
    planes_classify(s, sp, pos, n, inA, cls);
    long badCls = 0;
    for (int q = 0; q < n; q += step) {
        int x0, y0, x1, y1, f = 0, b = 0;
        hero_rect(s, h, pos[q], &x0, &y0, &x1, &y1);
        y1 += 8; /* l'ombre déborde sous le héros */
        for (int y = y0 < 0 ? 0 : y0; y < y1 && y < s->height; y++)
            for (int x = x0 < 0 ? 0 : x0; x < x1 && x < s->width; x++) {
                if (!inA[y * s->width + x])
                    continue;
                const int32_t o = s->owner[y * s->width + x];
                int front;
                if (inside(s, h, pos[q], x, y))
                    front = voxel_in_front(s, o, h, pos[q]);
                else if (inside(s, &sp->shadow, pos[q], x, y))
                    front = voxel_in_front(s, o, &sp->shadow, pos[q]);
                else
                    continue;
                f += front;
                b += !front;
            }
        const int want = !b ? PRIO_LOW : !f ? PRIO_HIGH : PRIO_MASKED;
        badCls += want != cls[q];
    }
    CHECK(badCls == 0, "%s : %ld positions mal classées", name, badCls);
    free(cls);
    free(inA);
    free(pos);
}

/* 4. Les tiles redonnent la scène */
static void check_tiles(const Scene *s, const Cubes *c, const char *name) {
    const Sprites *sp = &SPRITES_DEFAULT;
    const int n = positions_list(s->vox, &sp->hero, NULL, NULL, 0);
    Pos *pos = malloc(sizeof *pos * (size_t)(n + 1));
    positions_list(s->vox, &sp->hero, pos, NULL, n);
    uint8_t *inA = malloc((size_t)s->width * s->height);
    planes_assign(s, sp, pos, n, inA);
    MdPalette pal;
    palette_build(c->rgb, &pal);
    static MdTiles t;
    tiles_build(s, inA, &pal, &t);
    uint8_t *rgb = malloc((size_t)s->width * s->height * 3);
    tiles_render(&t, &pal, s->width, s->height, rgb);
    long diff = 0;
    for (int p = 0; p < s->width * s->height; p++) {
        const uint8_t idx = s->color[p];
        const uint8_t *want = idx ? pal.rgb[palette_line(idx)][palette_color(idx)]
                                  : pal.rgb[0][0];
        diff += memcmp(want, rgb + 3 * p, 3) != 0;
    }
    CHECK(diff == t.approxPixels,
          "%s : %ld pixels changés par les tiles, %d annoncés", name, diff,
          t.approxPixels);
    /* Plan A prioritaire partout, plan B jamais */
    int prioA = 1, prioB = 0;
    for (int e = 0; e < PLANE_W * PLANE_H; e++) {
        prioA &= t.planeA[e] >> 15;
        prioB |= t.planeB[e] >> 15;
    }
    CHECK(prioA && !prioB, "%s : bits de priorité des plans", name);
    free(rgb);
    tiles_free(&t);
    free(inA);
    free(pos);
}

int main(void) {
    static Cubes cubes;
    cubes_load("demo/zone/cubes.txt", "demo/assets", &cubes);

    /* La zone de démo */
    static Voxels demo;
    vox_load("demo/zone/demo.vox", &demo);
    Scene s;
    scene_render(&s, &demo, &cubes);
    long pixels = check_raycast(&s, "démo"), compared = 0;
    check_planes(&s, "démo", &compared, 3);
    check_tiles(&s, &cubes, "démo");
    scene_free(&s);

    /* Zones au hasard : sol, piliers, ponts, surplombs */
    int zones = 0;
    for (int z = 0; z < 100; z++) {
        Voxels v;
        v.W = 3 + rnd(6);
        v.D = 3 + rnd(6);
        v.H = 2 + rnd(5);
        v.voxels = calloc((size_t)v.W * v.D * v.H, 1);
        for (int j = 0; j < v.D; j++)
            for (int i = 0; i < v.W; i++)
                v.voxels[j * v.W + i] = (uint8_t)(1 + rnd(6));
        const int fill = 5 + rnd(30);
        for (int n = 0; n < v.W * v.D * v.H * fill / 100; n++)
            v.voxels[(rnd(v.H) * v.D + rnd(v.D)) * v.W + rnd(v.W)] =
                (uint8_t)(1 + rnd(6));
        char name[32];
        snprintf(name, sizeof name, "zone %d", z);
        scene_render(&s, &v, &cubes);
        pixels += check_raycast(&s, name);
        if (z % 4 == 0)
            check_planes(&s, name, &compared, 1);
        if (s.width <= 512 && s.height <= 512)
            check_tiles(&s, &cubes, name);
        scene_free(&s);
        free(v.voxels);
        zones++;
    }
    printf("test_scene : démo et %d zones ; %ld pixels contre le lancer de "
           "rayons, %ld contre le rayon du héros : %s\n",
           zones, pixels, compared, failures ? "ÉCHEC" : "OK");
    free(demo.voxels);
    return failures != 0;
}
