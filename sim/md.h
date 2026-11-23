#ifndef SIM_MD_H
#define SIM_MD_H

#include <stddef.h>
#include <stdint.h>

#include "vdp.h"

/* =========================================================================
   La Mega Drive du banc sans affichage : le 68000 (Musashi), la ROM, la
   RAM, le VDP (vdp.h) et une manette 3 boutons. Le Z80 et le son ne sont
   pas modélisés (la ROM laisse le Z80 arrêté).

   Temps : NTSC, 262 lignes de 488 cycles du 68000 (127 856 cycles par
   image) ; le VBlank commence à la ligne 224, avec la V_INT (niveau 6).

   La ROM de Cobra publie à l'adresse $200 : 'COBR', l'adresse où elle
   revient quand le travail d'une image est fait (avant d'attendre le VBlank), celle de l'état du héros en RAM et celle du
   compteur d'images. Le banc mesure à chaque image les cycles écoulés
   entre la V_INT et ce retour : le travail de la V_INT et de la boucle
   principale.
   ========================================================================= */

#define MD_LINE_CYCLES 488
#define MD_LINES 262
#define MD_FRAME_CYCLES (MD_LINE_CYCLES * MD_LINES)

/* Boutons, comme la ROM les range (1 = enfoncé) */
enum {
    PAD_UP = 1,
    PAD_DOWN = 2,
    PAD_LEFT = 4,
    PAD_RIGHT = 8,
    PAD_B = 16,
    PAD_C = 32,
    PAD_A = 64,
    PAD_START = 128
};

typedef struct {
    uint8_t *rom;
    size_t romSize;
    uint8_t ram[65536];
    Vdp vdp;
    uint8_t padTh;  /* dernière valeur écrite sur le port de la manette */
    uint16_t pad;   /* boutons enfoncés */
    uint64_t cycles;
    uint32_t donePc, heroAddr, frameAddr;
    int measuring;
    uint64_t vintCycle;
    uint32_t busy; /* cycles de la dernière mesure (0 : pas encore) */
    uint32_t measures; /* nombre de mesures faites */
    int overshoot; /* cycles déjà exécutés en trop sur la ligne */
    int badAccess; /* accès hors de ce que le modèle connaît */
    uint32_t badAddress;
} Md;

/* Charge la ROM et fait un reset ; 0 si tout va bien */
int md_open(Md *m, const char *romPath);
void md_close(Md *m);

/* Une image entière, la manette dans l'état 'pad' */
void md_frame(Md *m, uint16_t pad);

uint16_t md_ram16(const Md *m, uint32_t address);
void md_set_ram16(Md *m, uint32_t address, uint16_t value);
uint32_t md_ram32(const Md *m, uint32_t address);

#endif
