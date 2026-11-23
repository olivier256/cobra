#!/usr/bin/env python3
"""
Écrit la zone de démo de Cobra au format MagicaVoxel (.vox), pour qu'on
puisse l'ouvrir et la modifier dans MagicaVoxel.

    python3 tools/demozone.py demo/zone/demo.vox

Zone de 17 x 15 cases, 8 niveaux : son image fait 512 x 384 pixels, elle
tient dans un plan de 64 x 64 tiles (W + D = 32 ; D impair pour que les
cubes tombent sur le treillis, voir iso/iso.h). Le numéro de couleur d'un
voxel est son type de cube (voir demo/zone/cubes.txt) :
  1 dalle, 2 pierre, 3 colonne, 4 eau, 5 pont, 6 caisse.
- le sol : deux niveaux de dalles (on marche à la hauteur 2) ;
- un canal (j = 7 et 8) : de l'eau au niveau 0, un niveau sous le sol ;
- un pont (i = 4 et 5) au niveau 2 : une marche au-dessus du sol ;
- une arche de pierre (i = 11, j = 2 à 4) : deux piliers et un linteau
  au niveau 5 ; on passe dessous ;
- une colonnade (j = 10 et 12) : des colonnes bleues de trois niveaux,
  pour les cas « entre deux colonnes » ;
- un plateau de deux niveaux de plus (i = 12 à 16, j = 9 à 14), avec une
  marche devant ;
- un mur de pierre au fond (j = 0 et i = 0), et des caisses.

Format .vox (MagicaVoxel, version 150) : en-tête « VOX », version, puis
le bloc MAIN qui contient SIZE, XYZI (x, y, z, couleur) et RGBA.
x, y, z de MagicaVoxel = i, j, k ; z vers le haut.
"""

import struct
import sys

W, D, H = 17, 15, 8
DALLE, PIERRE, COLONNE, EAU, PONT, CAISSE = 1, 2, 3, 4, 5, 6
COLORS = {DALLE: (180, 180, 180), PIERRE: (144, 144, 160),
          COLONNE: (72, 108, 180), EAU: (36, 72, 144), PONT: (72, 108, 216),
          CAISSE: (108, 144, 216)}


def build():
    v = {}

    def put(i, j, k, t):
        if 0 <= i < W and 0 <= j < D and 0 <= k < H:
            v[(i, j, k)] = t

    canal = (7, 8)
    for i in range(W):
        for j in range(D):
            if j in canal:
                put(i, j, 0, EAU)
            else:
                put(i, j, 0, DALLE)
                put(i, j, 1, DALLE)
    # Mur du fond, trois niveaux, avec une porte en i = 7
    for i in range(W):
        for k in (2, 3, 4):
            if not (i in (7, 8) and k < 4):
                put(i, 0, k, PIERRE)
    for j in range(D):
        if j in canal:
            continue
        for k in (2, 3):
            put(0, j, k, PIERRE)
    # Pont sur le canal, une marche au-dessus du sol
    for i in (4, 5):
        for j in range(6, 10):
            put(i, j, 2, PONT)
    # Arche : piliers en j = 2 et 4, linteau de j = 2 à 4 au niveau 5
    for j in (2, 4):
        for k in (2, 3, 4):
            put(11, j, k, PIERRE)
    for j in range(2, 5):
        put(11, j, 5, PIERRE)
    # Colonnade : deux rangées de colonnes de trois niveaux
    for j in (10, 12):
        for i in (3, 5, 7, 9):
            for k in (2, 3, 4):
                put(i, j, k, COLONNE)
    # Plateau : deux niveaux de plus, et une marche devant
    for i in range(12, 17):
        for j in range(9, 15):
            put(i, j, 2, PIERRE)
            put(i, j, 3, DALLE)
    for j in range(10, 14):
        put(11, j, 2, PIERRE)
    # Caisses
    put(4, 3, 2, CAISSE)
    put(5, 3, 2, CAISSE)
    put(5, 3, 3, CAISSE)
    return v


def chunk(name, content, children=b""):
    return name + struct.pack("<ii", len(content), len(children)) + content + children


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "demo/zone/demo.vox"
    voxels = build()
    size = chunk(b"SIZE", struct.pack("<iii", W, D, H))
    xyzi = struct.pack("<i", len(voxels)) + b"".join(
        struct.pack("<BBBB", i, j, k, t) for (i, j, k), t in sorted(voxels.items()))
    palette = b""
    for n in range(1, 257):
        r, g, b = COLORS.get(n, (64, 64, 64))
        palette += struct.pack("<BBBB", r, g, b, 255)
    children = size + chunk(b"XYZI", xyzi) + chunk(b"RGBA", palette)
    data = b"VOX " + struct.pack("<i", 150) + chunk(b"MAIN", b"", children)
    open(out, "wb").write(data)
    print("%s : %d x %d x %d, %d voxels" % (out, W, D, H, len(voxels)))


if __name__ == "__main__":
    main()
