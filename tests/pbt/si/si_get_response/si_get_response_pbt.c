#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

typedef int32_t s32;
typedef int BOOL;

BOOL SIGetResponse(s32 chan, void *data);
void gc_si_set_status_seed(uint32_t chan, uint32_t status);
void gc_si_set_resp_words_seed(uint32_t chan, uint32_t word0, uint32_t word1);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void store_u32be(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static uint32_t load_u32be(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return 0;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    enum {
        SI_ERROR_RDST = 0x0020u,
        SI_INPUTBUF_VALID_BASE = 0x801A7148u,
        SI_INPUTBUF_BASE = 0x801A7158u,
    };

    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();

    for (uint32_t i = 0; i < iters; i++) {
        uint32_t chan = xs32(&seed) % 4u;
        uint32_t status = xs32(&seed);
        uint32_t w0 = xs32(&seed);
        uint32_t w1 = xs32(&seed);
        uint32_t stale0 = xs32(&seed);
        uint32_t stale1 = xs32(&seed);
        uint32_t prevalid = xs32(&seed) & 1u;

        uint32_t disable_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_DISABLE_CALLS, 0);
        uint32_t restore_before = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_RESTORE_CALLS, 0);

        gc_si_set_status_seed(chan, status);
        gc_si_set_resp_words_seed(chan, w0, w1);

        // Preload InputBuffer state to test stale valid-consume path when RDST is not set.
        store_u32be(SI_INPUTBUF_BASE + chan * 8u + 0u, stale0);
        store_u32be(SI_INPUTBUF_BASE + chan * 8u + 4u, stale1);
        store_u32be(SI_INPUTBUF_VALID_BASE + chan * 4u, prevalid);

        const uint32_t out_addr = 0x80301000u;
        store_u32be(out_addr + 0u, 0xAAAAAAAAu);
        store_u32be(out_addr + 4u, 0xBBBBBBBBu);
        BOOL got = SIGetResponse((s32)chan, (void *)(uintptr_t)out_addr);

        uint32_t rdst = (status & SI_ERROR_RDST) ? 1u : 0u;
        uint32_t exp_valid = rdst ? 1u : prevalid;
        uint32_t exp0 = rdst ? w0 : stale0;
        uint32_t exp1 = rdst ? w1 : stale1;

        if ((uint32_t)got != exp_valid) {
            fprintf(stderr, "PBT FAIL step=%u: return mismatch got=%u exp=%u\n", i, (uint32_t)got, exp_valid);
            return 1;
        }
        if (exp_valid) {
            uint32_t got0 = load_u32be(out_addr + 0u);
            uint32_t got1 = load_u32be(out_addr + 4u);
            if (got0 != exp0 || got1 != exp1) {
                fprintf(stderr, "PBT FAIL step=%u: data mismatch got=(%08X,%08X) exp=(%08X,%08X)\n",
                        i, got0, got1, exp0, exp1);
                return 1;
            }
        } else {
            uint32_t got0 = load_u32be(out_addr + 0u);
            uint32_t got1 = load_u32be(out_addr + 4u);
            if (got0 != 0xAAAAAAAAu || got1 != 0xBBBBBBBBu) {
                fprintf(stderr, "PBT FAIL step=%u: output mutated while invalid\n", i);
                return 1;
            }
        }

        uint32_t post_valid = load_u32be(SI_INPUTBUF_VALID_BASE + chan * 4u);
        if (post_valid != 0u) {
            fprintf(stderr, "PBT FAIL step=%u: valid flag not cleared\n", i);
            return 1;
        }

        uint32_t disable_after = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_DISABLE_CALLS, 0);
        uint32_t restore_after = gc_sdk_state_load_u32_or(GC_SDK_OFF_OS_RESTORE_CALLS, 0);
        if (disable_after != disable_before + 1u || restore_after != restore_before + 1u) {
            fprintf(stderr, "PBT FAIL step=%u: interrupt wrapping mismatch\n", i);
            return 1;
        }
    }

    printf("PBT PASS: si_get_response %u iterations\n", iters);
    return 0;
}
