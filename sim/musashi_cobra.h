/* Configuration de Musashi pour le banc de Cobra : un 68000, et un appel
   à md_instruction_hook (sim/md.c) avant chaque instruction. Inclus avant
   tout (-include) : les fichiers de Musashi restent tels quels. */
#ifndef MUSASHI_COBRA_H
#define MUSASHI_COBRA_H
#define M68K_EMULATE_010 M68K_OPT_OFF
#define M68K_EMULATE_EC020 M68K_OPT_OFF
#define M68K_EMULATE_020 M68K_OPT_OFF
#define M68K_EMULATE_030 M68K_OPT_OFF
#define M68K_EMULATE_040 M68K_OPT_OFF
#define M68K_INSTRUCTION_HOOK M68K_OPT_SPECIFY_HANDLER
#define M68K_INSTRUCTION_CALLBACK(pc) md_instruction_hook(pc)
void md_instruction_hook(unsigned int pc);
#endif
