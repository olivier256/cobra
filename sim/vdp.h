#ifndef SIM_VDP_H
#define SIM_VDP_H

#include <stdint.h>

/* =========================================================================
   Un modèle du VDP de la Mega Drive, pour le banc sans affichage : ce que
   la ROM de Cobra utilise, et rien d'autre.

   - Ports : registres, écritures en VRAM, CRAM et VSRAM par le port de
     données, auto-incrément. Le DMA et les lectures par le port de données
     ne sont pas modélisés : la ROM qui s'en sert est arrêtée (vdp.error).
   - Image : mode 5, 320 ou 256 pixels, 224 lignes ; plans A et B (tailles
     32 à 128), défilement plein écran, par tile ou par ligne, vertical
     plein écran ou par colonne de 16 pixels ; sprites (liste chaînée, 20
     par ligne, 320 pixels par ligne en mode 320) ; mode ombre et
     surbrillance. Pas de plan fenêtre ni de masquage des sprites (x = 0).

   Règles du mode ombre et surbrillance retenues (documentation de Charles
   MacDonald, à confronter à un émulateur précis) :
   - un pixel des plans ou du fond est assombri si ni le tile du plan A ni
     celui du plan B à cet endroit n'a le bit de priorité ; la priorité
     d'un tile compte même là où il est transparent ;
   - un sprite prioritaire est éclairé normalement ; un sprite non
     prioritaire prend l'éclairage du décor sous lui ;
   - les couleurs 14 et 15 de la ligne 3 d'un sprite ne s'affichent pas :
     elles éclairent (14) ou assombrissent (15) le pixel du dessous, s'il
     n'est pas devant elles (plan prioritaire opaque sur sprite non
     prioritaire).
   Intensités, sur 0 à 14 : normale 2c, ombre c, surbrillance 7 + c (c :
   niveau de la CRAM, 0 à 7).
   ========================================================================= */

#define VDP_W 320
#define VDP_H 224

typedef struct {
    uint8_t reg[32];
    uint8_t vram[65536];
    uint16_t cram[64];
    uint16_t vsram[40];
    uint16_t addr;
    uint8_t code;
    int pending; /* premier mot d'une commande reçu */
    int vblank;  /* pour le registre d'état */
    int error;   /* 1 : la ROM a demandé ce que le modèle ne fait pas */
    const char *errorText;
} Vdp;

void vdp_reset(Vdp *v);
void vdp_write_ctrl(Vdp *v, uint16_t w);
void vdp_write_data(Vdp *v, uint16_t w);
uint16_t vdp_read_ctrl(Vdp *v);
uint16_t vdp_read_data(Vdp *v);

/* L'image : rgb (VDP_W x VDP_H x 3) en intensités 0 à 14 ; sprites : 0 pour
   ne pas les dessiner ; source (VDP_W x VDP_H, peut être nul) : numéro + 1
   du sprite affiché à chaque pixel (hors opérateurs), 0 sinon */
void vdp_render(const Vdp *v, uint8_t *rgb, int sprites, uint8_t *source);

/* Défilement plein écran du plan A (pour les tests) */
int vdp_scroll_x(const Vdp *v);
int vdp_scroll_y(const Vdp *v);

#endif
