/* =========================================================================
   Les positions du héros, la répartition entre les plans A et B, la
   priorité du sprite à chaque position (voir conv.h).
   ========================================================================= */

#include "conv.h"

static int solid(const Voxels *v, int i, int j, int k) {
    return vox_at(v, i, j, k) != 0;
}

int positions_list(const Voxels *v, const HeroShape *h, int step, Pos *out,
                   uint8_t *walk, int max) {
    int n = 0;
    const int first = step == 1 ? 0 : step / 2;
    for (int k = 1; k < v->H; k++)
        for (int vv = first; vv + h->half <= 16 * v->D; vv += step)
            for (int u = first; u + h->half <= 16 * v->W; u += step) {
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

static int in_rect(const Scene *s, const HeroShape *h, Pos p, int x, int y) {
    int x0, y0, x1, y1;
    hero_rect(s, h, p, &x0, &y0, &x1, &y1);
    return x >= x0 && x < x1 && y >= y0 && y < y1;
}

int pixel_in_front(const Scene *s, const Sprites *sp, int32_t o, int x, int y,
                   Pos p) {
    if (o < 0)
        return 0;
    if (!in_rect(s, &sp->hero, p, x, y) && in_rect(s, &sp->shadow, p, x, y))
        return voxel_in_front(s, o, &sp->shadow, p);
    return voxel_in_front(s, o, &sp->hero, p);
}

/* Les pixels des rectangles des deux sprites, une fois chacun */
#define FOR_SPRITE_PIXELS(s, sp, p, x, y, body)                                \
    do {                                                                       \
        for (int which = 0; which < 2; which++) {                              \
            const HeroShape *sh_ = which ? &(sp)->shadow : &(sp)->hero;        \
            int x0, y0, x1, y1;                                                \
            hero_rect(s, sh_, p, &x0, &y0, &x1, &y1);                          \
            clip_rect(s, &x0, &y0, &x1, &y1);                                  \
            for (int y = y0; y < y1; y++)                                      \
                for (int x = x0; x < x1; x++) {                                \
                    if (which && in_rect(s, &(sp)->hero, p, x, y))             \
                        continue;                                              \
                    body                                                       \
                }                                                              \
        }                                                                      \
    } while (0)

void planes_assign(const Scene *s, const Sprites *sp, const Pos *pos, int n,
                   uint8_t *inA) {
    for (int p = 0; p < s->width * s->height; p++)
        inA[p] = 0;
    for (int q = 0; q < n; q++)
        FOR_SPRITE_PIXELS(s, sp, pos[q], x, y, {
            const int o = y * s->width + x;
            if (!inA[o] && pixel_in_front(s, sp, s->owner[o], x, y, pos[q]))
                inA[o] = 1;
        });
}

void planes_classify(const Scene *s, const Sprites *sp, const Pos *pos, int n,
                     const uint8_t *inA, uint8_t *cls) {
    for (int q = 0; q < n; q++) {
        int front = 0, behind = 0;
        FOR_SPRITE_PIXELS(s, sp, pos[q], x, y, {
            const int o = y * s->width + x;
            if (inA[o]) {
                if (pixel_in_front(s, sp, s->owner[o], x, y, pos[q]))
                    front = 1;
                else
                    behind = 1;
            }
        });
        cls[q] = behind ? (front ? PRIO_MASKED : PRIO_HIGH) : PRIO_LOW;
    }
}

int masks_build(const Scene *s, const Sprites *sp, Pos p, const uint8_t *inA,
                MdTiles *t, Mask *out, int max) {
    /* Les tiles touchés par les rectangles des deux sprites */
    int tx0 = 1 << 30, ty0 = 1 << 30, tx1 = -1, ty1 = -1;
    for (int which = 0; which < 2; which++) {
        int x0, y0, x1, y1;
        hero_rect(s, which ? &sp->shadow : &sp->hero, p, &x0, &y0, &x1, &y1);
        clip_rect(s, &x0, &y0, &x1, &y1);
        if (x0 >= x1 || y0 >= y1)
            continue;
        if (x0 / 8 < tx0)
            tx0 = x0 / 8;
        if (y0 / 8 < ty0)
            ty0 = y0 / 8;
        if ((x1 - 1) / 8 > tx1)
            tx1 = (x1 - 1) / 8;
        if ((y1 - 1) / 8 > ty1)
            ty1 = (y1 - 1) / 8;
    }
    int n = 0;
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            uint8_t px[64];
            int line, any = 0, touches = 0;
            tiles_plane_a(t, tx, ty, px, &line);
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    const int X = tx * 8 + x, Y = ty * 8 + y;
                    const int o = Y * s->width + X;
                    const int front =
                        inA[o] && px[y * 8 + x] &&
                        pixel_in_front(s, sp, s->owner[o], X, Y, p);
                    if (!front) {
                        px[y * 8 + x] = 0;
                        continue;
                    }
                    any = 1;
                    touches |= in_rect(s, &sp->hero, p, X, Y) ||
                               in_rect(s, &sp->shadow, p, X, Y);
                }
            if (!any || !touches)
                continue;
            int flags;
            const int index = tiles_add(t, px, &flags);
            if (n < max) {
                out[n].tx = (uint8_t)tx;
                out[n].ty = (uint8_t)ty;
                out[n].attr =
                    (uint16_t)((line << 13) | (flags << 11) | index);
            }
            n++;
        }
    return n;
}
