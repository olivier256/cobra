/* =========================================================================
   La Mega Drive du banc (voir md.h).
   ========================================================================= */

#include "md.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"

static Md *cur; /* Musashi appelle des fonctions globales */

static uint32_t rom32(const Md *m, uint32_t a) {
    if (a + 4 > m->romSize)
        return 0;
    return ((uint32_t)m->rom[a] << 24) | (m->rom[a + 1] << 16) |
           (m->rom[a + 2] << 8) | m->rom[a + 3];
}

int md_open(Md *m, const char *romPath) {
    memset(m, 0, sizeof *m);
    FILE *f = fopen(romPath, "rb");
    if (f == NULL)
        return 1;
    fseek(f, 0, SEEK_END);
    m->romSize = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    m->rom = malloc(m->romSize);
    if (m->rom == NULL || fread(m->rom, 1, m->romSize, f) != m->romSize) {
        fclose(f);
        return 1;
    }
    fclose(f);
    if (m->romSize < 0x210 || memcmp(m->rom + 0x200, "COBR", 4) != 0)
        return 2;
    m->donePc = rom32(m, 0x204);
    m->heroAddr = rom32(m, 0x208) & 0xFFFF;
    m->frameAddr = rom32(m, 0x20C) & 0xFFFF;
    vdp_reset(&m->vdp);
    m->padTh = 0x40;
    cur = m;
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();
    return 0;
}

void md_close(Md *m) {
    free(m->rom);
    m->rom = NULL;
    if (cur == m)
        cur = NULL;
}

uint16_t md_ram16(const Md *m, uint32_t a) {
    a &= 0xFFFE;
    return (uint16_t)((m->ram[a] << 8) | m->ram[a + 1]);
}

void md_set_ram16(Md *m, uint32_t a, uint16_t v) {
    a &= 0xFFFE;
    m->ram[a] = (uint8_t)(v >> 8);
    m->ram[a + 1] = (uint8_t)v;
}

uint32_t md_ram32(const Md *m, uint32_t a) {
    return ((uint32_t)md_ram16(m, a) << 16) | md_ram16(m, a + 2);
}

/* ---- Bus ------------------------------------------------------------------ */

static void bad(uint32_t a) {
    if (!cur->badAccess) {
        cur->badAccess = 1;
        cur->badAddress = a;
    }
}

static uint8_t pad_read(void) {
    const uint16_t p = cur->pad;
    if (cur->padTh & 0x40) /* TH = 1 : C B R L D U */
        return (uint8_t)(0x40 | (~p & 0x3F));
    /* TH = 0 : S A 0 0 D U */
    return (uint8_t)(~(((p >> 2) & 0x30) | (p & 3)) & 0x33);
}

static unsigned io_read8(uint32_t a) {
    switch (a & 0x1F) {
    case 0x01:
        return 0xA0; /* étranger, NTSC, pas d'extension, version 0 */
    case 0x03:
        return pad_read();
    case 0x09:
        return 0x40;
    default:
        return 0;
    }
}

unsigned int m68k_read_memory_16(unsigned int a);

unsigned int m68k_read_memory_8(unsigned int a) {
    a &= 0xFFFFFF;
    if (a < 0x400000)
        return a < cur->romSize ? cur->rom[a] : 0xFF;
    if (a >= 0xE00000)
        return cur->ram[a & 0xFFFF];
    if (a >= 0xA10000 && a < 0xA10020)
        return io_read8(a);
    if (a >= 0xA11100 && a < 0xA11102)
        return 0;
    if (a >= 0xC00000 && a < 0xC00020) {
        const unsigned w = m68k_read_memory_16(a & ~1u);
        return (a & 1) ? (w & 0xFF) : (w >> 8);
    }
    bad(a);
    return 0;
}

unsigned int m68k_read_memory_16(unsigned int a) {
    a &= 0xFFFFFE;
    if (a < 0x400000)
        return a + 1 < cur->romSize ? (cur->rom[a] << 8) | cur->rom[a + 1]
                                    : 0xFFFF;
    if (a >= 0xE00000)
        return md_ram16(cur, a);
    if (a >= 0xC00000 && a < 0xC00020) {
        if (a < 0xC00004)
            return vdp_read_data(&cur->vdp);
        if (a < 0xC00008)
            return vdp_read_ctrl(&cur->vdp);
        return 0; /* compteur HV */
    }
    if (a >= 0xA10000 && a < 0xA10020)
        return io_read8(a + 1);
    if (a >= 0xA11100 && a < 0xA11102)
        return 0;
    bad(a);
    return 0;
}

unsigned int m68k_read_memory_32(unsigned int a) {
    return (m68k_read_memory_16(a) << 16) | m68k_read_memory_16(a + 2);
}

void m68k_write_memory_16(unsigned int a, unsigned int v);

void m68k_write_memory_8(unsigned int a, unsigned int v) {
    a &= 0xFFFFFF;
    v &= 0xFF;
    if (a >= 0xE00000) {
        cur->ram[a & 0xFFFF] = (uint8_t)v;
        return;
    }
    if (a >= 0xA10000 && a < 0xA10020) {
        if ((a & 0x1F) == 0x03)
            cur->padTh = (uint8_t)v;
        return;
    }
    if (a >= 0xC00000 && a < 0xC00020) {
        if ((a & 0x1F) == 0x11 || (a & 0x1F) == 0x13)
            return; /* PSG */
        m68k_write_memory_16(a & ~1u, (v << 8) | v);
        return;
    }
    if ((a >= 0xA11100 && a < 0xA11102) || (a >= 0xA11200 && a < 0xA11202) ||
        (a >= 0xA14000 && a < 0xA14004))
        return;
    bad(a);
}

void m68k_write_memory_16(unsigned int a, unsigned int v) {
    a &= 0xFFFFFE;
    v &= 0xFFFF;
    if (a >= 0xE00000) {
        md_set_ram16(cur, a, (uint16_t)v);
        return;
    }
    if (a >= 0xC00000 && a < 0xC00020) {
        if (a < 0xC00004)
            vdp_write_data(&cur->vdp, (uint16_t)v);
        else if (a < 0xC00008)
            vdp_write_ctrl(&cur->vdp, (uint16_t)v);
        return;
    }
    if (a >= 0xA10000 && a < 0xA10020) {
        m68k_write_memory_8(a + 1, v & 0xFF);
        return;
    }
    if ((a >= 0xA11100 && a < 0xA11102) || (a >= 0xA11200 && a < 0xA11202) ||
        (a >= 0xA14000 && a < 0xA14004))
        return;
    bad(a);
}

void m68k_write_memory_32(unsigned int a, unsigned int v) {
    m68k_write_memory_16(a, v >> 16);
    m68k_write_memory_16(a + 2, v & 0xFFFF);
}

unsigned int m68k_read_disassembler_8(unsigned int a) {
    return m68k_read_memory_8(a);
}
unsigned int m68k_read_disassembler_16(unsigned int a) {
    return m68k_read_memory_16(a);
}
unsigned int m68k_read_disassembler_32(unsigned int a) {
    return m68k_read_memory_32(a);
}

/* Appelée par Musashi avant chaque instruction (voir musashi_cobra.h) */
void md_instruction_hook(unsigned int pc) {
    if (cur->measuring && pc == cur->donePc) {
        cur->busy = (uint32_t)(cur->cycles + (uint64_t)m68k_cycles_run() -
                               cur->vintCycle);
        cur->measuring = 0;
        cur->measures++;
    }
}

/* ---- Une image -------------------------------------------------------------- */

void md_frame(Md *m, uint16_t pad) {
    cur = m;
    m->pad = pad;
    for (int line = 0; line < MD_LINES; line++) {
        if (line == 0)
            m->vdp.vblank = 0;
        if (line == 224) {
            m->vdp.vblank = 1;
            if (m->vdp.reg[1] & 0x20) {
                m68k_set_irq(6);
                m->measuring = 1;
                m->vintCycle = m->cycles;
            }
        }
        const int budget = MD_LINE_CYCLES - m->overshoot;
        const int ran = budget > 0 ? m68k_execute(budget) : 0;
        m->cycles += (uint64_t)ran;
        m->overshoot = ran - budget;
    }
}
