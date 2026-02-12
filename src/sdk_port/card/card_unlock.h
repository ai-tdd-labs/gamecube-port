/*
 * sdk_port/card/card_unlock.h --- Host-side port of CARD unlock crypto helpers.
 *
 * Pure scalar computation (LFSR scramble, bit reversal, LCG PRNG).
 * No gc_mem needed — these operate on native u32 values.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/card/CARDUnlock.c
 */
#pragma once

#include <stdint.h>

uint32_t port_exnor_1st(uint32_t data, uint32_t rshift);
uint32_t port_exnor(uint32_t data, uint32_t lshift);
uint32_t port_bitrev(uint32_t data);

void port_CARDSrand(uint32_t seed);
int  port_CARDRand(void);
