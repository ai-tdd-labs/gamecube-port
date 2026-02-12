/*
 * card_unlock_oracle.h --- Oracle for CARD unlock crypto property tests.
 *
 * Exact copy of decomp exnor_1st, exnor, bitrev, CARDRand/CARDSrand
 * with oracle_ prefix.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/card/CARDUnlock.c
 */
#pragma once

#include <stdint.h>

/* ── exnor_1st (CARDUnlock.c:63-75, exact copy) ── */
static uint32_t oracle_exnor_1st(uint32_t data, uint32_t rshift)
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

/* ── exnor (CARDUnlock.c:77-91, exact copy) ── */
static uint32_t oracle_exnor(uint32_t data, uint32_t lshift)
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

/* ── bitrev (CARDUnlock.c:93-117, exact copy) ── */
static uint32_t oracle_bitrev(uint32_t data)
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

/* ── CARDSrand / CARDRand (CARDUnlock.c:39-48, exact copy) ── */
static unsigned long int oracle_next = 1;

static void oracle_CARDSrand(uint32_t seed)
{
    oracle_next = seed;
}

static int oracle_CARDRand(void)
{
    oracle_next = oracle_next * 1103515245 + 12345;
    return (int)((unsigned int)(oracle_next / 65536) % 32768);
}
