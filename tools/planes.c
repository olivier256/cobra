/* =========================================================================
   Les positions du héros, la répartition entre les plans A et B, la
   priorité du sprite à chaque position (voir conv.h).
   ========================================================================= */

#include "conv.h"

static int solid(const Voxels *v, int i, int j, int k) {
    return vox_at(v, i, j, k) != 0;
}

int positions_list(const Voxels *v, const HeroShape *h, Pos *out,
                   uint8_t *walk, int max) {
    int n = 0;
    for (int k = 1; k < v->H; k++)
        for (int vv = 2; vv + h->half <= 16 * v->D; vv += POS_STEP)
            for (int u = 2; u + h->half <= 16 * v->W; u += POS_STEP) {
                if (u - h->half < 0 || vv - h->half < 0)
                    continue;
                const int w = 16 * k;
                const int i0 = (u - h->half) >> 4, i1 = (u + h->half - 1) >> 4;
                const int j0 = (vv - h->half) >> 4,
                          j1 = (vv + h->half - 1) >> 4;
                const int k1 = (w + h->height - 1) >> 4;
                int free = 1, onGround = 0, oneAbove = 0;
                for (int j = j0; j <= j1; j++)
                    for (int i = i0; i <= i1; i++) {
                        for (int kk = k; kk <= k1; kk++)
                            free &= !solid(v, i, j, kk);
                        onGround |= solid(v, i, j, k - 1);
                        oneAbove |= solid(v, i, j, k - 2);
                    }
                if (!free || !(onGround || oneAbove))
                    continue;
                if (n < max) {
                    out[n].u = u;
                    out[n].v = vv;
                    out[n].w = w;
                    if (walk != 0)
                        walk[n] = (uint8_t)onGround;
                }
                n++;
            }
    return n;
}

static void clip_rect(const Scene *s, int *x0, int *y0, int *x1, int *y1) {
    if (*x0 < 0)
        *x0 = 0;
    if (*y0 < 0)
        *y0 = 0;
    if (*x1 > s->width)
        *x1 = s->width;
    if (*y1 > s->height)
        *y1 = s->height;
}

void planes_assign(const Scene *s, const HeroShape *h, const Pos *pos, int n,
                   uint8_t *inA) {
    for (int p = 0; p < s->width * s->height; p++)
        inA[p] = 0;
    for (int q = 0; q < n; q++) {
        int x0, y0, x1, y1;
        hero_rect(s, h, pos[q], &x0, &y0, &x1, &y1);
        clip_rect(s, &x0, &y0, &x1, &y1);
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) {
                const int o = y * s->width + x;
                if (!inA[o] && s->owner[o] >= 0 &&
                    voxel_in_front(s, s->owner[o], h, pos[q]))
                    inA[o] = 1;
            }
    }
}

void planes_classify(const Scene *s, const HeroShape *h, const Pos *pos,
                     int n, const uint8_t *inA, uint8_t *cls) {
    for (int q = 0; q < n; q++) {
        int x0, y0, x1, y1, front = 0, behind = 0;
        hero_rect(s, h, pos[q], &x0, &y0, &x1, &y1);
        clip_rect(s, &x0, &y0, &x1, &y1);
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) {
                const int o = y * s->width + x;
                if (!inA[o])
                    continue;
                if (voxel_in_front(s, s->owner[o], h, pos[q]))
                    front++;
                else
                    behind++;
            }
        if (!behind)
            cls[q] = PRIO_LOW;
        else if (!front)
            cls[q] = PRIO_HIGH;
        else
            cls[q] = behind > front ? PRIO_CONFLICT_HIGH : PRIO_CONFLICT_LOW;
    }
}
