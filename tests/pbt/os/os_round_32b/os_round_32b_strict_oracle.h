/*
 * os_round_32b_strict_oracle.h
 *
 * Strict decomp-derived leaf oracle for OSRoundUp32B/OSRoundDown32B.
 * Source of truth: decomp_mario_party_4/src/dolphin/os/OSArena.c
 */
#pragma once

#include <stdint.h>

static uint32_t strict_OSRoundUp32B(uint32_t x) {
    return (x + 31u) & ~31u;
}

static uint32_t strict_OSRoundDown32B(uint32_t x) {
    return x & ~31u;
}
