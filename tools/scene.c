/* =========================================================================
   La scène : l'image de la zone, pixel par pixel, et le héros dedans.
   ========================================================================= */

#include "conv.h"

#include <stdlib.h>
#include <string.h>

#include "iso.h"

void scene_render(Scene *s, const Voxels *v, const Cubes *c) {
    s->vox = v;
    /* Géométrie de l'image (comme dds32conv) : originX / 16 + originY / 8
       pair, pour que les cubes tombent sur le treillis */
    const int pad = ((v->D - 1) & 1) ? ISO_TRI_W : 0;
    s->originX = ISO_TRI_W * (v->D - 1) + pad;
    s->originY = ISO_UNITS * (v->H - 1);
    s->width = ISO_TRI_W * (v->W + v->D) + pad;
    s->height = 8 * (v->W + v->D) + ISO_UNITS * v->H;
    const size_t n = (size_t)s->width * s->height;
    s->color = calloc(n, 1);
    s->owner = malloc(n * sizeof *s->owner);
    for (size_t p = 0; p < n; p++)
        s->owner[p] = -1;

    const int maxSum = v->W + v->D + v->H - 3;
    for (int sum = 0; sum <= maxSum; sum++)
        for (int k = 0; k < v->H; k++)
            for (int i = 0; i < v->W; i++) {
                const int j = sum - i - k;
                if (j < 0 || j >= v->D)
                    continue;
                const int32_t idx = (k * v->D + j) * v->W + i;
                const uint8_t type = v->voxels[idx];
                if (type == 0)
                    continue;
                if (!c->present[type])
                    bmp_fail("un type de cube est absent de CUBES.txt%s", "");
                const int X = ISO_TRI_W * (i - j) + s->originX;
                const int Y = 8 * (i + j) - ISO_UNITS * k + s->originY;
                for (int y = 0; y < CUBE_SIZE; y++)
                    for (int x = 0; x < CUBE_SIZE; x++) {
                        const uint8_t px = c->sprite[type][y * CUBE_SIZE + x];
                        if (px == 0)
                            continue;
                        const size_t o = (size_t)(Y + y) * s->width + X + x;
                        s->color[o] = px;
                        s->owner[o] = idx;
                    }
            }
}

void scene_free(Scene *s) {
    free(s->color);
    free(s->owner);
    s->color = NULL;
    s->owner = NULL;
}

/* ---- Rayons --------------------------------------------------------------
   Le rayon du pixel (x, y), pris en (x + 1/2, y + 1) : c'est la convention
   des triangles du treillis (iso/iso.h), dont la rangée r fait 2 (r + 1)
   pixels de large, la largeur exacte de la face au bas de la rangée. Avec
   ce point, le lancer de rayons et l'ordre du peintre donnent la même image
   au pixel près, arêtes comprises (tests/test_scene). Les points (u, v, w)
   du rayon :
   u - v = a et (u + v) / 2 - w = b, soit u = b + w + a / 2,
   v = b + w - a / 2, paramétrés par w : plus w est grand, plus le point
   est près de l'œil. Une boîte coupe le rayon sur un intervalle de w ;
   le bout le plus proche de l'œil est sa borne haute. */

typedef struct {
    double a, b;
} Ray;

static Ray ray_of(const Scene *s, int x, int y) {
    Ray r = {x + 0.5 - s->originX - 16, y + 1.0 - s->originY - 16};
    return r;
}

/* Intervalle [lo, hi[ de w où le rayon est dans la boîte ; 0 s'il est vide */
static int ray_box(Ray r, double u0, double u1, double v0, double v1,
                   double w0, double w1, double *hi) {
    double lo = w0, h = w1;
    const double ul = u0 - r.b - r.a / 2, uh = u1 - r.b - r.a / 2;
    const double vl = v0 - r.b + r.a / 2, vh = v1 - r.b + r.a / 2;
    if (ul > lo)
        lo = ul;
    if (vl > lo)
        lo = vl;
    if (uh < h)
        h = uh;
    if (vh < h)
        h = vh;
    *hi = h;
    return lo < h;
}

static int32_t ray_first(const Scene *s, Ray r, double *nearest) {
    const Voxels *v = s->vox;
    int32_t best = -1;
    double bestHi = 0;
    for (int k = 0; k < v->H; k++)
        for (int j = 0; j < v->D; j++)
            for (int i = 0; i < v->W; i++) {
                const int32_t idx = (k * v->D + j) * v->W + i;
                if (v->voxels[idx] == 0)
                    continue;
                double hi;
                if (ray_box(r, 16 * i, 16 * i + 16, 16 * j, 16 * j + 16,
                            16 * k, 16 * k + 16, &hi) &&
                    (best < 0 || hi > bestHi)) {
                    best = idx;
                    bestHi = hi;
                }
            }
    *nearest = bestHi;
    return best;
}

int32_t scene_raycast(const Scene *s, int x, int y) {
    double hi;
    return ray_first(s, ray_of(s, x, y), &hi);
}

int scene_ray_hits(const Scene *s, int x, int y, int32_t voxel, double *near) {
    const Voxels *v = s->vox;
    const int i = voxel % v->W, j = (voxel / v->W) % v->D,
              k = voxel / (v->W * v->D);
    return ray_box(ray_of(s, x, y), 16 * i, 16 * i + 16, 16 * j, 16 * j + 16,
                   16 * k, 16 * k + 16, near);
}

/* ---- Le héros ----------------------------------------------------------- */

void hero_rect(const Scene *s, const HeroShape *h, Pos p, int *x0, int *y0,
               int *x1, int *y1) {
    const int sx = p.u - p.v + s->originX + 16;
    const int sy = ((p.u + p.v) >> 1) - p.w + s->originY + 16;
    *x0 = sx - h->spriteW / 2;
    *y0 = sy - h->footRow;
    *x1 = *x0 + h->spriteW;
    *y1 = *y0 + h->spriteH;
}

int voxel_in_front(const Scene *s, int32_t voxel, const HeroShape *h, Pos p) {
    const Voxels *v = s->vox;
    const int i = voxel % v->W, j = (voxel / v->W) % v->D,
              k = voxel / (v->W * v->D);
    const int front = 16 * i >= p.u + h->half || 16 * j >= p.v + h->half ||
                      16 * k >= p.w + h->height;
    const int behind = 16 * i + 16 <= p.u - h->half ||
                       16 * j + 16 <= p.v - h->half || 16 * k + 16 <= p.w;
    return front && !behind;
}

int ray_voxel_in_front(const Scene *s, int x, int y, const HeroShape *h,
                       Pos p) {
    const Ray r = ray_of(s, x, y);
    double heroHi, voxHi;
    if (!ray_box(r, p.u - h->half, p.u + h->half, p.v - h->half,
                 p.v + h->half, p.w, p.w + h->height, &heroHi))
        return -1;
    if (ray_first(s, r, &voxHi) < 0 || voxHi == heroHi)
        return -1;
    return voxHi > heroHi;
}
