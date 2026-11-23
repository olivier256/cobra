/* =========================================================================
   romsum : écrit la somme de contrôle dans l'en-tête d'une ROM Mega Drive
   (somme des mots de $200 à la fin, rangée en $18E). La console ne la
   vérifie pas, mais certains émulateurs le signalent.

   Usage : romsum ROM
   ========================================================================= */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage : romsum ROM\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "r+b");
    if (f == NULL) {
        fprintf(stderr, "romsum : impossible d'ouvrir %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    if (size < 0x200 || size % 2) {
        fprintf(stderr, "romsum : %s : taille refusée\n", argv[1]);
        return 1;
    }
    uint8_t *rom = malloc((size_t)size);
    fseek(f, 0, SEEK_SET);
    if (rom == NULL || fread(rom, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "romsum : lecture impossible\n");
        return 1;
    }
    uint16_t sum = 0;
    for (long a = 0x200; a < size; a += 2)
        sum = (uint16_t)(sum + ((rom[a] << 8) | rom[a + 1]));
    fseek(f, 0x18E, SEEK_SET);
    fputc(sum >> 8, f);
    fputc(sum & 255, f);
    fclose(f);
    free(rom);
    return 0;
}
