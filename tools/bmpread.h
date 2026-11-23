#ifndef BMPREAD_H
#define BMPREAD_H

#include <stdint.h>

/* =========================================================================
   Lecture des BMP 8 bits indexés (outils bmp2c et dds32conv) : BMP Windows
   non compressé, 8 bits par pixel, de haut en bas ou de bas en haut. C'est
   ce qu'enregistrent GIMP (mode indexé), Aseprite, Photoshop ou Pillow en
   mode "P".
   ========================================================================= */

typedef struct {
    int width, height;
    uint8_t *pixels;     /* width * height, de haut en bas */
    uint8_t rgb[256][3]; /* palette du fichier */
    int colors;          /* entrées présentes dans le fichier */
} BmpImage;

/* Lit 'path' ; en cas d'erreur, message et sortie du programme */
void bmp_load(const char *path, BmpImage *img);

/* Nom de l'outil, en tête des messages : chaque outil le définit */
extern const char *bmp_tool_name;

/* Message d'erreur (fmt contient au plus un %s) et sortie du programme */
void bmp_fail(const char *fmt, const char *arg);

#endif
