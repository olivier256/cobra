#!/usr/bin/env python3
"""
Dessine les graphismes provisoires de Cobra : les cubes isométriques de
32 x 32 pixels du décor, la silhouette du héros (16 x 32), en BMP 8 bits
indexés (Pillow), avec une palette commune.

    python3 tools/cubes2bmp.py demo/assets

Géométrie (voir iso/iso.h) : une case fait un losange de 32 x 16 pixels,
un niveau 16 pixels ; le cube est un hexagone découpé en six triangles.

Palette (voir ARCHITECTURE.md, « Palettes ») : chaque couleur est une
couleur de la Mega Drive (8 niveaux par canal, niveau x 36) ; l'index dit
dans quelle ligne de palette elle ira :
  0       transparent
  1       fond (couleur 0 de la ligne 0, celle du « backdrop »)
  16-26   ligne 0, couleurs 1 à 11 : décor gris
  32-42   ligne 1, couleurs 1 à 11 : décor bleu
  48-63   ligne 2, couleurs 0 à 15 : le héros (rouge, bottes noires)
Les couleurs 12 à 15 des lignes 0, 1 et 2 sont réservées au rayon delta
(du jaune au blanc). Un cube ne mélange pas les lignes : sinon les tiles
de ses arêtes en mêleraient deux.
"""

import random
import sys

from PIL import Image

SIZE = 32

GRIS = [(1, 1, 1), (2, 2, 2), (2, 2, 3), (3, 3, 3), (3, 3, 4), (4, 4, 4),
        (4, 4, 5), (5, 5, 5), (5, 5, 6), (6, 6, 6), (7, 7, 7)]
BLEU = [(0, 0, 1), (0, 0, 2), (1, 1, 2), (1, 1, 3), (1, 2, 3), (1, 2, 4),
        (2, 2, 4), (2, 3, 5), (2, 3, 6), (3, 4, 6), (4, 5, 7)]
RAYON = [(7, 5, 0), (7, 7, 0), (7, 7, 4), (7, 7, 7)]
HEROS = [(0, 0, 0), (0, 0, 0), (1, 0, 0), (3, 0, 0), (5, 0, 0), (6, 1, 1),
         (7, 2, 2), (7, 4, 4), (2, 2, 2), (4, 4, 4), (0, 0, 0), (0, 0, 0)]
FOND = (0, 0, 1)

RAMPS = {"gris": (16, GRIS), "bleu": (32, BLEU)}


def md(c):
    return tuple(36 * x for x in c)


def palette():
    pal = [(0, 0, 0)] * 256
    pal[1] = md(FOND)
    for first, colors in RAMPS.values():
        for i, c in enumerate(colors):
            pal[first + i] = md(c)
        for i, c in enumerate(RAYON):
            pal[first + 12 + i] = md(c)
    for i, c in enumerate(HEROS):
        pal[48 + i] = md(c)
    for i, c in enumerate(RAYON):
        pal[48 + 12 + i] = md(c)
    flat = []
    for c in pal:
        flat += c
    return flat


def shade(ramp, level):
    first, colors = RAMPS[ramp]
    return first + max(0, min(len(colors) - 1, level))


def iso_row_width(r):
    return 2 * (r + 1) if r < 8 else 2 * (15 - r)


SLOTS = {(0, 0): "top", (1, 0): "top", (0, 1): "left", (0, 2): "left",
         (1, 1): "right", (1, 2): "right"}


def region(x, y):
    """'top', 'left', 'right' ou None (transparent), d'après le treillis."""
    if not (0 <= x < SIZE and 0 <= y < SIZE):
        return None
    c, lx = x // 16, x % 16
    t, r = y // 8, y % 8
    for tt, rr in ((t, r), (t - 1, r + 8)):
        w = iso_row_width(rr)
        points_left = (c + tt) % 2 == 0
        start = 16 - w if points_left else 0
        if start <= lx < start + w:
            return SLOTS.get((c, tt))
    return None


def edge(x, y):
    """Vrai sur le contour de l'hexagone et sur les arêtes entre faces."""
    here = region(x, y)
    if here is None:
        return False
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nx, ny = x + dx, y + dy
        there = region(nx, ny) if (0 <= nx < SIZE and 0 <= ny < SIZE) else None
        if there is None:
            return True
    return False


def make_cube(ramp, top, left, right, outline):
    img = Image.new("P", (SIZE, SIZE), 0)
    img.putpalette(palette())
    rng = random.Random(SIZE)
    px = img.load()
    for y in range(SIZE):
        for x in range(SIZE):
            r = region(x, y)
            if r is None:
                continue
            if edge(x, y):
                px[x, y] = shade(ramp, outline)
            else:
                px[x, y] = {"top": top, "left": left, "right": right}[r](x, y, rng)
    return img


# ---- Matières ---------------------------------------------------------------

def slab_top(ramp, base):
    def f(x, y, rng):
        u, v = (x + 2 * y) % 16, (2 * y - x) % 16
        if u == 0 or v == 0:
            return shade(ramp, base - 3)
        return shade(ramp, base + rng.choice((-1, 0, 0, 0, 0)))
    return f


def bricks(ramp, base, left):
    def f(x, y, rng):
        k = x if left else 31 - x
        row = (y - k // 2) // 4
        joint_h = (y - k // 2) % 4 == 0
        joint_v = (k + (4 if row % 2 else 0)) % 8 == 0
        if joint_h or joint_v:
            return shade(ramp, base - 2)
        return shade(ramp, base + rng.choice((-1, 0, 0)))
    return f


def fluted(ramp, base, left):
    def f(x, y, rng):
        k = x if left else 31 - x
        if k % 4 == 0:
            return shade(ramp, base - 2)
        if k % 4 == 1:
            return shade(ramp, base + 1)
        return shade(ramp, base)
    return f


def panel(ramp, base, left):
    def f(x, y, rng):
        k = x if left else 31 - x
        yy = y - k // 2
        if k < 2 or k > 13 or yy < 10 or yy > 21:
            return shade(ramp, base + 1)
        if (k, yy) in ((3, 11), (12, 11), (3, 20), (12, 20)):
            return shade(ramp, base + 3)
        return shade(ramp, base - 1)
    return f


def grating_top(x, y, rng):
    if (x + 2 * y) % 6 in (0, 1):
        return shade("bleu", 3)
    return shade("bleu", 7)


def water_top(x, y, rng):
    if (x + 2 * y + rng.choice((0, 0, 0, 1))) % 11 == 0:
        return shade("bleu", 8)
    return shade("bleu", 4 + rng.choice((0, 0, 1)))


def flat(ramp, base):
    def f(x, y, rng):
        return shade(ramp, base + rng.choice((-1, 0, 0)))
    return f


def hero():
    """Silhouette provisoire, 16 x 32 : pieds en bas au milieu."""
    img = Image.new("P", (16, 32), 0)
    img.putpalette(palette())
    px = img.load()
    R = {"n": 49, "f": 51, "m": 52, "c": 53, "v": 54, "l": 55, "g": 56}
    rows = [
        "......ffff......",
        ".....fmmmmf.....",
        ".....mcccvm.....",
        ".....mcvvvm.....",
        ".....fmccmf.....",
        "......fmmf......",
        "....ffmccmff....",
        "...fmcccvvcmf...",
        "..fmccvvvvccmf..",
        "..fmcvvllvvcmf..",
        "..fmcvvllvvcmf..",
        "..fmccvvvvccmf..",
        "..fm.ccvvcc.mf..",
        "..fm.ccvvcc.mf..",
        "..ff.cccccc.ff..",
        "..gg.mccccm.gg..",
        ".....mmccmm.....",
        ".....fnnnnf.....",
        ".....mmmmmm.....",
        ".....mmm.mmm....",
        ".....mmm.mmm....",
        ".....mmf.fmm....",
        ".....mmf.fmm....",
        ".....mmf.fmm....",
        ".....nnn.nnn....",
        ".....nnn.nnn....",
        ".....nnn.nnn....",
        ".....nnn.nnn....",
        "....nnnn.nnnn...",
        "....nnnn.nnnn...",
        "................",
        "................",
    ]
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch != ".":
                px[x, y] = R[ch]
    return img


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "demo/assets"
    cubes = {
        "dalle": make_cube("gris", slab_top("gris", 7), flat("gris", 4),
                           flat("gris", 2), 1),
        "pierre": make_cube("gris", slab_top("gris", 8),
                            bricks("gris", 6, True), bricks("gris", 4, False), 1),
        "colonne": make_cube("bleu", slab_top("bleu", 9),
                             fluted("bleu", 7, True), fluted("bleu", 5, False), 1),
        "eau": make_cube("bleu", water_top, flat("bleu", 3), flat("bleu", 2), 1),
        "pont": make_cube("bleu", grating_top, panel("bleu", 6, True),
                          panel("bleu", 4, False), 1),
        "caisse": make_cube("bleu", slab_top("bleu", 8), panel("bleu", 7, True),
                            panel("bleu", 5, False), 1),
    }
    for name, img in cubes.items():
        img.save("%s/%s.bmp" % (out, name))
        print("%s/%s.bmp" % (out, name))
    hero().save("%s/heros.bmp" % out)
    print("%s/heros.bmp" % out)


if __name__ == "__main__":
    main()
