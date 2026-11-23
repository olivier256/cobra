#ifndef CONV_H
#define CONV_H

#include <stdint.h>

#include "bmpread.h"

/* =========================================================================
   cobraconv : le « Mirage Type III » de Cobra. Du monde de voxels
   (MagicaVoxel) aux tiles, plans et tables de la Mega Drive.

   Repère du monde (celui du DDS520, voir iso/iso.h) : voxel (i, j, k),
   i vers le bas à droite de l'écran, j vers le bas à gauche, k vers le
   haut ; 16 unités par case et par niveau : u = 16 i, v = 16 j, w = 16 k.
   Un point du monde (u, v, w) tombe dans l'image de la zone en
       x = u - v + originX + 16,   y = (u + v) / 2 - w + originY + 16.
   Plus u + v + w est grand, plus le point est près de l'œil.
   ========================================================================= */

/* ---- Voxels ------------------------------------------------------------- */

typedef struct {
    int W, D, H;
    uint8_t *voxels; /* voxels[(k * D + j) * W + i], 0 = vide */
} Voxels;

void vox_load(const char *path, Voxels *v);

static inline uint8_t vox_at(const Voxels *v, int i, int j, int k) {
    if (i < 0 || j < 0 || k < 0 || i >= v->W || j >= v->D || k >= v->H)
        return 0;
    return v->voxels[(k * v->D + j) * v->W + i];
}

/* ---- Cubes -------------------------------------------------------------- */

#define CUBE_SIZE 32

typedef struct {
    uint8_t present[256];
    uint8_t sprite[256][CUBE_SIZE * CUBE_SIZE]; /* index de la palette commune */
    uint8_t rgb[256][3];                        /* la palette commune */
} Cubes;

/* Lit CUBES.txt et les images DOSSIER/NOM.bmp */
void cubes_load(const char *path, const char *imageDir, Cubes *c);

/* ---- La scène : l'image de la zone, pixel par pixel --------------------- */

typedef struct {
    const Voxels *vox;
    int originX, originY;
    int width, height;
    uint8_t *color;  /* index de la palette commune ; 0 = rien */
    int32_t *owner;  /* voxel visible (index dans vox->voxels), -1 = rien */
} Scene;

/* Dessine la zone de l'arrière vers l'avant (par i + j + k, puis k) */
void scene_render(Scene *s, const Voxels *v, const Cubes *c);
void scene_free(Scene *s);

/* Référence : le voxel vu au centre du pixel (x, y), par un lancer de rayon
   (sans l'ordre du peintre) ; -1 = rien */
int32_t scene_raycast(const Scene *s, int x, int y);

/* Le rayon du pixel (x, y) touche-t-il ce voxel ? Si oui, 'near' reçoit
   la profondeur (w) de son point le plus proche de l'œil */
int scene_ray_hits(const Scene *s, int x, int y, int32_t voxel, double *near);

/* ---- Le héros ----------------------------------------------------------- */

/* Le héros est une boîte de (2 half) x (2 half) x height unités, posée en
   (u, v, w) (milieu des pieds). Son sprite est un rectangle de spriteW x
   spriteH pixels, ses pieds en (spriteW / 2, footRow) du sprite. */
typedef struct {
    int half, height;
    int spriteW, spriteH, footRow;
} HeroShape;

static const HeroShape HERO_DEFAULT = {4, 26, 16, 32, 30};

typedef struct {
    int u, v, w;
} Pos;

/* Rectangle du sprite dans l'image : [x0, x1[ x [y0, y1[ */
void hero_rect(const Scene *s, const HeroShape *h, Pos p, int *x0, int *y0,
               int *x1, int *y1);

/* Vrai si le voxel est devant le héros (boîtes séparées le long d'un axe
   vers l'œil) */
int voxel_in_front(const Scene *s, int32_t voxel, const HeroShape *h, Pos p);

/* Référence : le rayon du pixel (x, y) touche-t-il la boîte du héros, et
   le voxel visible est-il devant elle ? 0 = derrière, 1 = devant,
   -1 = le rayon ne touche pas le héros ou rien n'est visible */
int ray_voxel_in_front(const Scene *s, int x, int y, const HeroShape *h,
                       Pos p);

/* ---- Les positions du héros et les deux plans --------------------------- */

#define POS_STEP 4 /* unités entre deux positions essayées */

/* Les positions où le héros peut se trouver : boîte libre, posée sur un
   voxel (marche) ou un niveau au-dessus (saut). u = 4 m + 2, v = 4 n + 2,
   w = 16 k. Renvoie le nombre de positions ; 'out' en reçoit au plus max ;
   'walk' (si non nul) dit pour chacune si elle est posée. */
int positions_list(const Voxels *v, const HeroShape *h, Pos *out,
                   uint8_t *walk, int max);

/* Le héros et son ombre, posés à une même position. L'ombre : une boîte
   plate de la même empreinte, posée au sol sous le héros ; son sprite fait
   16 x 8, centré sur le point du sol. */
typedef struct {
    HeroShape hero, shadow;
} Sprites;

static const Sprites SPRITES_DEFAULT = {{4, 26, 16, 32, 30}, {4, 1, 16, 8, 4}};

/* Le pixel (x, y), de voxel o, est-il devant ce qui est dessiné là quand
   le héros est en p ? Dans le rectangle du héros : devant sa boîte ;
   sinon, dans celui de l'ombre : devant la boîte de l'ombre ; ailleurs :
   devant la boîte du héros. */
int pixel_in_front(const Scene *s, const Sprites *sp, int32_t o, int x, int y,
                   Pos p);

/* Plan A : un pixel de décor y va s'il peut cacher le héros ou son ombre à
   l'une des positions (le pixel est dans le rectangle d'un des deux
   sprites, son voxel devant la boîte correspondante). inA : width x height
   octets. */
void planes_assign(const Scene *s, const Sprites *sp, const Pos *pos, int n,
                   uint8_t *inA);

enum { PRIO_LOW = 0, PRIO_HIGH = 1, PRIO_MASKED = 2, PRIO_NONE = 0xFF };

/* Pour chaque position : les pixels du plan A dans les rectangles des deux
   sprites sont tous devant (priorité basse), tous derrière (priorité
   haute), ou des deux sortes (masqués : priorité haute, et des sprites
   « masques » recopient par-dessus les pixels du plan A qui sont devant,
   voir masks_build) */
void planes_classify(const Scene *s, const Sprites *sp, const Pos *pos, int n,
                     const uint8_t *inA, uint8_t *cls);

/* ---- Palettes, tiles et plans de la Mega Drive -------------------------- */

#define PALETTE_LINES 4
#define DECOR_COLORS 11 /* couleurs 1 à 11 des lignes 0 et 1 */
#define PLANE_W 64      /* plan de 64 x 64 tiles */
#define PLANE_H 64

typedef struct {
    uint8_t rgb[PALETTE_LINES][16][3]; /* niveaux 0 à 7 */
} MdPalette;

/* Palette de la Mega Drive d'après la palette commune (voir
   tools/cubes2bmp.py) ; échoue si une couleur n'est pas une couleur de la
   Mega Drive */
void palette_build(const uint8_t rgb[256][3], MdPalette *pal);

/* Ligne (0 ou 1) et couleur (1 à 11) d'un index de décor ; -1 pour le
   fond ou le transparent */
int palette_line(uint8_t index);
int palette_color(uint8_t index);

typedef struct {
    int tileCount; /* tiles de 32 octets ; le tile 0 est vide */
    int maxTiles;
    uint8_t (*tiles)[32];
    int tableSize;
    int *table; /* table de hachage des tiles rangés (numéro + 1) */
    uint16_t planeA[PLANE_W * PLANE_H], planeB[PLANE_W * PLANE_H];
    /* mesures */
    int tilesNoFlip;          /* sans les retournements */
    int usedA, usedB;         /* entrées non vides de chaque plan */
    int mixedTiles;           /* tiles qui mêlaient les deux lignes */
    int approxPixels;         /* pixels changés de couleur */
} MdTiles;

/* Découpe l'image en tiles 8 x 8 pour les deux plans ; plan A prioritaire
   partout (tiles vides comprises), plan B jamais */
void tiles_build(const Scene *s, const uint8_t *inA, const MdPalette *pal,
                 MdTiles *t);
void tiles_free(MdTiles *t);

/* Ce qu'affiche le VDP (sans sprites) : RGB (niveaux 0 à 7) de l'image
   des deux plans, width x height x 3 */
void tiles_render(const MdTiles *t, const MdPalette *pal, int width,
                  int height, uint8_t *rgb);

/* Range un tile (s'il n'y est pas déjà, retournements compris) ; renvoie
   son numéro, et dans *flags les retournements à appliquer (bit 0 :
   horizontal, bit 1 : vertical) */
int tiles_add(MdTiles *t, const uint8_t px[64], int *flags);

/* Les pixels du tile (tx, ty) du plan A, tels qu'affichés (retournements
   appliqués), et sa ligne de palette */
void tiles_plane_a(const MdTiles *t, int tx, int ty, uint8_t px[64],
                   int *line);

/* ---- Masques -------------------------------------------------------------
   À une position masquée, un sprite de 8 x 8 par tile du plan A qui
   recouvre le héros ou son ombre et contient des pixels devant eux : il
   recopie ces pixels-là (les autres sont transparents), placé sur la grille
   des tiles, avant le héros dans la liste des sprites et prioritaire. */

typedef struct {
    uint8_t tx, ty; /* tile de l'image de la zone */
    uint16_t attr;  /* ligne, retournements, numéro du tile (sans priorité) */
} Mask;

/* Les masques de la position p ; renvoie leur nombre (au plus max) ; les
   tiles « devant seulement » sont ajoutés à t */
int masks_build(const Scene *s, const Sprites *sp, Pos p, const uint8_t *inA,
                MdTiles *t, Mask *out, int max);

/* 4 bits par pixel, gros-boutiste : un tile de 8 x 8 */
void tile_pack(const uint8_t px[64], uint8_t out[32]);
void tile_unpack(const uint8_t in[32], uint8_t px[64]);

#endif
