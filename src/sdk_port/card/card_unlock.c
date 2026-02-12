/*
 * sdk_port/card/card_unlock.c --- Host-side port of CARD unlock crypto helpers.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/card/CARDUnlock.c
 *
 * exnor_1st (lines 63-75):  EXNOR LFSR right-shift scramble
 * exnor     (lines 77-91):  EXNOR LFSR left-shift scramble
 * bitrev    (lines 93-117): 32-bit reversal
 * CARDSrand (lines 45-48):  LCG seed
 * CARDRand  (lines 39-43):  LCG PRNG
 */
#include <stdint.h>
#include "card_unlock.h"

/* ── exnor_1st (CARDUnlock.c:63-75) ── */

uint32_t port_exnor_1st(uint32_t data, uint32_t rshift)
{
    uint32_t wk;
    uint32_t w;
    uint32_t i;

    w = data;
    for (i = 0; i < rshift; i++) {
        wk = ~(w ^ (w >> 7) ^ (w >> 15) ^ (w >> 23));
        w = (w >> 1) | ((wk << 30) & 0x40000000);
    }
    return w;
}

/* ── exnor (CARDUnlock.c:77-91) ── */

uint32_t port_exnor(uint32_t data, uint32_t lshift)
{
    uint32_t wk;
    uint32_t w;
    uint32_t i;

    w = data;
    for (i = 0; i < lshift; i++) {
        wk = ~(w ^ (w << 7) ^ (w << 15) ^ (w << 23));
        w = (w << 1) | ((wk >> 30) & 0x00000002);
    }
    return w;
}

/* ── bitrev (CARDUnlock.c:93-117) ── */

uint32_t port_bitrev(uint32_t data)
{
    uint32_t wk;
    uint32_t i;
    uint32_t k = 0;
    uint32_t j = 1;

    wk = 0;
    for (i = 0; i < 32; i++) {
        if (i > 15) {
            if (i == 31) {
                wk |= (((data & (0x01u << 31)) >> 31) & 0x01);
            }
            else {
                wk |= ((data & (0x01u << i)) >> j);
                j += 2;
            }
        }
        else {
            wk |= ((data & (0x01u << i)) << (31 - i - k));
            k++;
        }
    }
    return wk;
}

/* ── CARDSrand / CARDRand (CARDUnlock.c:39-48) ── */

static unsigned long int port_next = 1;

void port_CARDSrand(uint32_t seed)
{
    port_next = seed;
}

int port_CARDRand(void)
{
    port_next = port_next * 1103515245 + 12345;
    return (int)((unsigned int)(port_next / 65536) % 32768);
}
