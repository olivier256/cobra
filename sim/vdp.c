/* =========================================================================
   Modèle du VDP (voir vdp.h).
   ========================================================================= */

#include "vdp.h"

#include <string.h>

void vdp_reset(Vdp *v) { memset(v, 0, sizeof *v); }

static void fail(Vdp *v, const char *text) {
    if (!v->error) {
        v->error = 1;
        v->errorText = text;
    }
}

void vdp_write_ctrl(Vdp *v, uint16_t w) {
    if (v->pending) {
        v->addr = (uint16_t)((v->addr & 0x3FFF) | ((w & 3) << 14));
        v->code = (uint8_t)((v->code & 3) | ((w >> 2) & 0x3C));
        v->pending = 0;
        if (v->code & 0x20)
            fail(v, "DMA (non modélisé)");
        return;
    }
    if ((w & 0xC000) == 0x8000) {
        v->reg[(w >> 8) & 0x1F] = (uint8_t)w;
        return;
    }
    v->code = (uint8_t)((v->code & 0x3C) | (w >> 14));
    v->addr = (uint16_t)((v->addr & 0xC000) | (w & 0x3FFF));
    v->pending = 1;
}

void vdp_write_data(Vdp *v, uint16_t w) {
    v->pending = 0;
    switch (v->code & 0x0F) {
    case 1: { /* VRAM */
        const uint16_t a = v->addr & 0xFFFE;
        const int swap = v->addr & 1;
        v->vram[a] = (uint8_t)(swap ? w : w >> 8);
        v->vram[a + 1] = (uint8_t)(swap ? w >> 8 : w);
        break;
    }
    case 3: /* CRAM */
        v->cram[(v->addr >> 1) & 0x3F] = w & 0x0EEE;
        break;
    case 5: /* VSRAM */
        if ((v->addr >> 1) < 40)
            v->vsram[v->addr >> 1] = w & 0x7FF;
        break;
    default:
        fail(v, "écriture sur le port de données sans commande d'écriture");
        return;
    }
    v->addr = (uint16_t)(v->addr + v->reg[15]);
}

uint16_t vdp_read_ctrl(Vdp *v) {
    v->pending = 0;
    return (uint16_t)(0x3400 | (v->vblank ? 8 : 0));
}

uint16_t vdp_read_data(Vdp *v) {
    fail(v, "lecture par le port de données (non modélisée)");
    return 0;
}

static uint16_t rd16(const Vdp *v, uint32_t a) {
    a &= 0xFFFE;
    return (uint16_t)((v->vram[a] << 8) | v->vram[a + 1]);
}

static int tile_pixel(const Vdp *v, int tile, int x, int y) {
    const uint8_t b = v->vram[(tile * 32 + y * 4 + (x >> 1)) & 0xFFFF];
    return (x & 1) ? (b & 15) : (b >> 4);
}

static int size_cells(int bits) {
    static const int s[4] = {32, 64, 32, 128};
    return s[bits & 3];
}

int vdp_scroll_x(const Vdp *v) {
    const int hs = (v->reg[13] & 0x3F) << 10;
    return (int16_t)(rd16(v, (uint32_t)hs) << 6) >> 6;
}

int vdp_scroll_y(const Vdp *v) { return v->vsram[0]; }

/* Pixel du plan (0 : A, 1 : B) : couleur (ligne x 16 + index, 0 si
   transparent) et priorité du tile */
static int plane_pixel(const Vdp *v, int plane, int x, int y, int *prio) {
    const int pw = size_cells(v->reg[16]), ph = size_cells(v->reg[16] >> 4);
    const uint32_t base = plane == 0 ? (uint32_t)(v->reg[2] & 0x38) << 10
                                     : (uint32_t)(v->reg[4] & 7) << 13;
    const uint32_t hs = (uint32_t)(v->reg[13] & 0x3F) << 10;
    int line = 0;
    switch (v->reg[11] & 3) {
    case 2:
        line = y & ~7;
        break;
    case 3:
        line = y;
        break;
    default:
        break;
    }
    const int hscroll = rd16(v, hs + (uint32_t)line * 4 + plane * 2) & 0x3FF;
    const int vscroll = (v->reg[11] & 4)
                            ? v->vsram[((x >> 4) * 2 + plane) % 40] & 0x3FF
                            : v->vsram[plane] & 0x3FF;
    const int px = (x - hscroll) & (pw * 8 - 1);
    const int py = (y + vscroll) & (ph * 8 - 1);
    const uint16_t e = rd16(v, base + (uint32_t)((py >> 3) * pw + (px >> 3)) * 2);
    *prio = e >> 15;
    const int tx = (e & 0x800) ? 7 - (px & 7) : (px & 7);
    const int ty = (e & 0x1000) ? 7 - (py & 7) : (py & 7);
    const int c = tile_pixel(v, e & 0x7FF, tx, ty);
    return c ? ((e >> 13) & 3) * 16 + c : 0;
}

static void put(const Vdp *v, uint8_t *out, int color, int mode) {
    const uint16_t c = v->cram[color & 63];
    const int lv[3] = {(c >> 1) & 7, (c >> 5) & 7, (c >> 9) & 7};
    for (int k = 0; k < 3; k++)
        out[k] = (uint8_t)(mode == 0 ? 2 * lv[k] : mode < 0 ? lv[k] : 7 + lv[k]);
}

void vdp_render(const Vdp *v, uint8_t *rgb, int sprites, uint8_t *source) {
    const int h40 = v->reg[12] & 1;
    const int width = h40 ? 320 : 256;
    const int sh = (v->reg[12] & 8) != 0;
    const uint32_t sat = (uint32_t)(v->reg[5] & (h40 ? 0x7E : 0x7F)) << 9;
    const int maxSprites = h40 ? 80 : 64, perLine = h40 ? 20 : 16;
    const int backdrop = v->reg[7] & 63;

    memset(rgb, 0, VDP_W * VDP_H * 3);
    if (source != NULL)
        memset(source, 0, VDP_W * VDP_H);
    for (int y = 0; y < VDP_H; y++) {
        /* Les sprites de la ligne, du premier (devant) au dernier */
        int spr[VDP_W], sprPrio[VDP_W], sprNum[VDP_W];
        memset(spr, 0, sizeof spr);
        memset(sprPrio, 0, sizeof sprPrio);
        memset(sprNum, 0, sizeof sprNum);
        if (sprites) {
            int n = 0, count = 0, budget = width;
            for (int guard = 0; guard < maxSprites; guard++) {
                const uint32_t a = sat + (uint32_t)n * 8;
                const int sy = (rd16(v, a) & 0x3FF) - 128;
                const int size = v->vram[(a + 2) & 0xFFFF];
                const int link = v->vram[(a + 3) & 0xFFFF] & 0x7F;
                const uint16_t attr = rd16(v, a + 4);
                const int sx = (rd16(v, a + 6) & 0x1FF) - 128;
                const int hs = ((size >> 2) & 3) + 1, vs = (size & 3) + 1;
                if (y >= sy && y < sy + vs * 8 && budget > 0 &&
                    count < perLine) {
                    count++;
                    const int row = y - sy;
                    for (int px = 0; px < hs * 8 && budget > 0; px++, budget--) {
                        const int x = sx + px;
                        if (x < 0 || x >= width || spr[x])
                            continue;
                        int col = px >> 3, r = row >> 3;
                        int tx = px & 7, ty = row & 7;
                        if (attr & 0x800) {
                            col = hs - 1 - col;
                            tx = 7 - tx;
                        }
                        if (attr & 0x1000) {
                            r = vs - 1 - r;
                            ty = 7 - ty;
                        }
                        const int tile = (attr & 0x7FF) + col * vs + r;
                        const int c = tile_pixel(v, tile, tx, ty);
                        if (c) {
                            spr[x] = ((attr >> 13) & 3) * 16 + c;
                            sprPrio[x] = attr >> 15;
                            sprNum[x] = n + 1;
                        }
                    }
                }
                if (link == 0)
                    break;
                n = link;
            }
        }
        for (int x = 0; x < width; x++) {
            int ap, bp;
            const int a = plane_pixel(v, 0, x, y, &ap);
            const int b = plane_pixel(v, 1, x, y, &bp);
            const int hiPlane = (ap && a) || (bp && b);
            int planePix;
            if (ap && a)
                planePix = a;
            else if (bp && b)
                planePix = b;
            else if (a)
                planePix = a;
            else if (b)
                planePix = b;
            else
                planePix = backdrop;
            const int bgShadow = sh && !ap && !bp;
            const int s = spr[x], sp = sprPrio[x];
            uint8_t *out = rgb + 3 * (y * VDP_W + x);
            if (s && (sp || !hiPlane)) {
                if (sh && (s == 62 || s == 63))
                    put(v, out, planePix,
                        s == 63 ? -1 : (bgShadow ? 0 : 1));
                else {
                    put(v, out, s, (sh && !sp && bgShadow) ? -1 : 0);
                    if (source != NULL)
                        source[y * VDP_W + x] = (uint8_t)sprNum[x];
                }
            } else {
                put(v, out, planePix, bgShadow ? -1 : 0);
            }
        }
    }
}
