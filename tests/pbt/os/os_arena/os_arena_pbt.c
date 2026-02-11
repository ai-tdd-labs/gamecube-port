#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

void *OSGetArenaLo(void);
void *OSGetArenaHi(void);
void OSSetArenaLo(void *addr);
void OSSetArenaHi(void *addr);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static uint32_t rand_mem1_addr(uint32_t *state) {
    // Keep generated pointers in MEM1 and non-zero to avoid fallback-path ambiguity.
    uint32_t v = 0x80000000u + (xs32(state) & 0x017FFFC0u);
    if (v == 0) v = 0x80000000u;
    return v;
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();

    // Deterministic baseline.
    OSSetArenaLo((void *)(uintptr_t)0x80004000u);
    OSSetArenaHi((void *)(uintptr_t)0x817FC000u);

    for (uint32_t i = 0; i < iters; i++) {
        uint32_t lo0 = rand_mem1_addr(&seed);
        uint32_t hi0 = rand_mem1_addr(&seed);
        uint32_t lo1 = rand_mem1_addr(&seed);
        uint32_t hi1 = rand_mem1_addr(&seed);

        OSSetArenaLo((void *)(uintptr_t)lo0);
        OSSetArenaHi((void *)(uintptr_t)hi0);

        uint32_t got_lo = (uint32_t)(uintptr_t)OSGetArenaLo();
        uint32_t got_hi = (uint32_t)(uintptr_t)OSGetArenaHi();
        if (got_lo != lo0 || got_hi != hi0) {
            fprintf(stderr,
                    "PBT FAIL: initial roundtrip lo=0x%08X hi=0x%08X got_lo=0x%08X got_hi=0x%08X\n",
                    lo0, hi0, got_lo, got_hi);
            return 1;
        }

        // Lo write must not mutate Hi.
        OSSetArenaLo((void *)(uintptr_t)lo1);
        got_lo = (uint32_t)(uintptr_t)OSGetArenaLo();
        got_hi = (uint32_t)(uintptr_t)OSGetArenaHi();
        if (got_lo != lo1 || got_hi != hi0) {
            fprintf(stderr,
                    "PBT FAIL: lo/hi independence lo1=0x%08X hi0=0x%08X got_lo=0x%08X got_hi=0x%08X\n",
                    lo1, hi0, got_lo, got_hi);
            return 1;
        }

        // Hi write must not mutate Lo.
        OSSetArenaHi((void *)(uintptr_t)hi1);
        got_lo = (uint32_t)(uintptr_t)OSGetArenaLo();
        got_hi = (uint32_t)(uintptr_t)OSGetArenaHi();
        if (got_lo != lo1 || got_hi != hi1) {
            fprintf(stderr,
                    "PBT FAIL: hi/lo independence lo1=0x%08X hi1=0x%08X got_lo=0x%08X got_hi=0x%08X\n",
                    lo1, hi1, got_lo, got_hi);
            return 1;
        }
    }

    printf("PBT PASS: os_arena %u iterations\n", iters);
    return 0;
}
