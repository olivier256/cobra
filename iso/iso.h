#ifndef ISO_H
#define ISO_H

#include <stdint.h>

/* =========================================================================
   DDS 32 : le treillis de triangles de la vue isométrique.

   Repère du monde (celui du DDS520 de Landstalker) : voxel (i, j, k),
   i vers le bas à droite de l'écran, j vers le bas à gauche, k vers le
   haut. Une case est un losange de 32 x 16 pixels, un niveau fait 16
   pixels. En unités du monde (16 par case) : u = 16 i, v = 16 j,
   w = 16 k.

   L'image d'une zone (W x D cases, H niveaux) se découpe en triangles de
   16 x 16 pixels, rangés en colonnes de 16 pixels de large :
   - le triangle (c, t) occupe la colonne c et les lignes 8 t à 8 t + 14 ;
   - il a un côté vertical de 16 pixels, sur le bord gauche ou droit de la
     colonne, et une pointe sur l'autre bord, 8 lignes plus bas ;
   - sa ligne r (0 à 15) fait iso_row_width(r) pixels : 2, 4, ... 16, 14,
     ... 2, 0, soit 128 pixels en tout ;
   - « pointe à gauche » (côté vertical à droite) quand c + t est pair : sa
     ligne r couvre [16 c + 16 - largeur, 16 c + 16[ ; sinon il couvre
     [16 c, 16 c + largeur[.
   Une ligne de pixels d'une colonne est partagée entre exactement deux
   triangles, (c, t) à sa ligne r et (c, t - 1) à sa ligne r + 8, avec
   t = y / 8 et r = y % 8 : leurs largeurs font 16 à elles deux. Tous les
   bords tombent sur des abscisses paires : on copie des mots entiers.

   Un cube (un voxel plein) couvre six triangles : deux colonnes, trois
   triangles chacune. Dessus : (0, 0) et (1, 0) ; face gauche (celle qui
   regarde vers +j) : (0, 1) et (0, 2) ; face droite (vers +i) : (1, 1)
   et (1, 2), en coordonnées relatives au coin haut gauche du sprite
   32 x 32 du cube.
   ========================================================================= */

#define ISO_TRI_W 16    /* largeur d'une colonne de triangles */
#define ISO_TRI_ROWS 16 /* lignes d'un triangle (la dernière est vide) */
#define ISO_TRI_PIXELS 128
#define ISO_ROW_PITCH 8  /* écart vertical entre deux triangles */
#define ISO_CUBE_SIZE 32 /* sprite d'un cube */
#define ISO_UNITS 16     /* unités du monde par case */

enum { ISO_FACE_TOP, ISO_FACE_LEFT, ISO_FACE_RIGHT, ISO_FACES };

/* Largeur de la ligne r (0 à 15) d'un triangle */
static inline int iso_row_width(int r) {
    return r < 8 ? 2 * (r + 1) : 2 * (15 - r);
}

/* Position de la ligne r dans les 128 octets d'un triangle */
static inline int iso_row_offset(int r) {
    return r <= 8 ? r * (r + 1) : 128 - (15 - r) * (16 - r);
}

/* Vrai si le triangle (c, t) a son côté vertical à droite */
static inline int iso_points_left(int c, int t) { return ((c + t) & 1) == 0; }

/* Début de la ligne r du triangle (c, t) dans sa colonne (0 à 16) */
static inline int iso_row_start(int c, int t, int r) {
    return iso_points_left(c, t) ? ISO_TRI_W - iso_row_width(r) : 0;
}

/* Les six triangles d'un cube : colonne, rangée, face */
static const struct {
    uint8_t dc, dt, face;
} iso_cube_slots[6] = {
    {0, 0, ISO_FACE_TOP},  {1, 0, ISO_FACE_TOP},   {0, 1, ISO_FACE_LEFT},
    {0, 2, ISO_FACE_LEFT}, {1, 1, ISO_FACE_RIGHT}, {1, 2, ISO_FACE_RIGHT},
};

#endif
