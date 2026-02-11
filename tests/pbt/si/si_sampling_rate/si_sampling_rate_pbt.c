#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

void SISetSamplingRate(uint32_t msec);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static const struct {
    uint16_t line;
    uint8_t count;
} XYNTSC[12] = {
    {263 - 17, 2}, {15, 18}, {30, 9}, {44, 6},  {52, 5},  {65, 4},
    {87, 3},       {87, 3},  {87, 3}, {131, 2}, {131, 2}, {131, 2},
};

static const struct {
    uint16_t line;
    uint8_t count;
} XYPAL[12] = {
    {313 - 17, 2}, {15, 21}, {29, 11}, {45, 7},  {52, 6},  {63, 5},
    {78, 4},       {104, 3}, {104, 3}, {104, 3}, {104, 3}, {156, 2},
};

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();

    for (uint32_t i = 0; i < iters; i++) {
        uint32_t msec_in = xs32(&seed) % 40u;
        uint32_t tv = xs32(&seed) % 8u; // includes unknown formats
        uint16_t vi54 = (uint16_t)(xs32(&seed) & 1u);

        gc_sdk_state_store_u32be(GC_SDK_OFF_VI_TV_FORMAT, tv);
        gc_sdk_state_store_u16be_mirror(GC_SDK_OFF_VI_REGS_U16BE + (54u * 2u), 0, vi54);

        uint32_t setxy_calls_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_SI_SETXY_CALLS, 0);
        uint32_t disable_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_DISABLE_CALLS, 0);
        uint32_t restore_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_RESTORE_CALLS, 0);

        SISetSamplingRate(msec_in);

        uint32_t msec = (msec_in > 11u) ? 11u : msec_in;
        int known_ntsc = (tv == 0u || tv == 2u || tv == 5u);
        int known_pal = (tv == 1u);
        const uint16_t line = known_pal ? XYPAL[msec].line : XYNTSC[known_ntsc ? msec : 0u].line;
        const uint8_t count = known_pal ? XYPAL[msec].count : XYNTSC[known_ntsc ? msec : 0u].count;
        const uint32_t factor = (vi54 & 1u) ? 2u : 1u;
        const uint32_t exp_line = factor * (uint32_t)line;
        const uint32_t exp_count = (uint32_t)count;

        uint32_t got_sampling = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SAMPLING_RATE);
        uint32_t got_line = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_LINE);
        uint32_t got_count = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_COUNT);
        uint32_t got_setxy_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_CALLS);
        uint32_t got_disable = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
        uint32_t got_restore = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);

        if (got_sampling != ((msec_in > 11u) ? 11u : msec_in)) {
            fprintf(stderr, "PBT FAIL step=%u: sampling got=%u exp=%u\n", i, got_sampling, (msec_in > 11u) ? 11u : msec_in);
            return 1;
        }
        if (got_line != exp_line || got_count != exp_count) {
            fprintf(stderr,
                    "PBT FAIL step=%u: setxy got_line=%u exp_line=%u got_count=%u exp_count=%u tv=%u msec_in=%u\n",
                    i, got_line, exp_line, got_count, exp_count, tv, msec_in);
            return 1;
        }
        if (got_setxy_calls != setxy_calls_before + 1u) {
            fprintf(stderr, "PBT FAIL step=%u: setxy_calls got=%u exp=%u\n", i, got_setxy_calls, setxy_calls_before + 1u);
            return 1;
        }
        if (got_disable != disable_before + 1u || got_restore != restore_before + 1u) {
            fprintf(stderr, "PBT FAIL step=%u: interrupt call counts mismatch\n", i);
            return 1;
        }
    }

    printf("PBT PASS: si_sampling_rate %u iterations\n", iters);
    return 0;
}
