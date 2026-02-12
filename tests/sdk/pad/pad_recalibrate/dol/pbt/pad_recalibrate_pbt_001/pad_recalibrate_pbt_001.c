#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;
typedef int BOOL;

BOOL PADRecalibrate(u32 mask);

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline uint32_t rotl1(uint32_t v) {
    return (v << 1) | (v >> 31);
}

static uint32_t rng_state;
static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

enum {
    L0_CASES = 12,
    L1_CASES = 12,
    L2_CASES = 12,
    L3_CASES = 512,
    L4_CASES = 32,
    L5_CASES = 10,
};

static const uint32_t k_masks[L0_CASES] = {
    0x00000000u, 0x80000000u, 0x40000000u, 0x20000000u,
    0x10000000u, 0x70000000u, 0xF0000000u, 0xFFFFFFFFu,
    0x60000000u, 0x30000000u, 0xC0000000u, 0x01010101u,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    uint32_t word_off = 0u;
    uint32_t total_cases = 0u;
    uint32_t master_hash = 0u;
    uint32_t l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    for (uint32_t i = 0; i < L0_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS, 0u);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK, 0u);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, 0u);
        BOOL ok = PADRecalibrate(k_masks[i]);
        uint32_t h = (uint32_t)ok;
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
        l0_hash = rotl1(l0_hash) ^ h;
    }
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    for (uint32_t i = 0; i < L1_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS, 0u);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK, 0u);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, 0u);
        BOOL ok0 = PADRecalibrate(k_masks[i]);
        BOOL ok1 = PADRecalibrate(k_masks[(i + 3u) % L0_CASES]);
        uint32_t h = ((uint32_t)ok0 << 1) ^ (uint32_t)ok1;
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
        l1_hash = rotl1(l1_hash) ^ h;
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    for (uint32_t i = 0; i < L2_CASES; i++) {
        uint32_t start = 0xA5A50000u | i;
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS, start);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK, ~start);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, start ^ 0xFFFFFFFFu);
        BOOL ok = PADRecalibrate(0x70000000u);
        uint32_t h = (uint32_t)ok;
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
        l2_hash = rotl1(l2_hash) ^ h;
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0x62A37EF1u;
    for (uint32_t i = 0; i < L3_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS, rng_next());
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK, rng_next());
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, rng_next());
        BOOL ok = PADRecalibrate(rng_next());
        uint32_t h = (uint32_t)ok;
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
        l3_hash = rotl1(l3_hash) ^ h;
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS, 0u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK, 0u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, 0u);
    for (uint32_t i = 0; i < L4_CASES; i++) {
        uint32_t mask = (i & 1u) ? 0x70000000u : 0x60000000u;
        BOOL ok = PADRecalibrate(mask);
        uint32_t h = (uint32_t)ok;
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
        l4_hash = rotl1(l4_hash) ^ h;
    }
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    static const uint32_t edges[L5_CASES] = {
        0x00000000u, 0x00000001u, 0x0000FFFFu, 0x7FFFFFFFu, 0x80000000u,
        0xFFFFFFFFu, 0x10000000u, 0x20000000u, 0x40000000u, 0x70000000u,
    };
    for (uint32_t i = 0; i < L5_CASES; i++) {
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS, 0u);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK, 0u);
        gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, 0u);
        BOOL ok = PADRecalibrate(edges[i]);
        uint32_t h = (uint32_t)ok;
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK);
        h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
        l5_hash = rotl1(l5_hash) ^ h;
    }
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    store_u32be_ptr(out + word_off * 4u, 0x5052434Cu); word_off++; // PRCL
    store_u32be_ptr(out + word_off * 4u, total_cases); word_off++;
    store_u32be_ptr(out + word_off * 4u, master_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, l0_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L0_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l1_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L1_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l2_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L2_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l3_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L3_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l4_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L4_CASES); word_off++;
    store_u32be_ptr(out + word_off * 4u, l5_hash); word_off++;
    store_u32be_ptr(out + word_off * 4u, L5_CASES); word_off++;

    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS, 10u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK, 0x12345678u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, 0x87654321u);
    BOOL ok = PADRecalibrate(0x70000000u);
    store_u32be_ptr(out + word_off * 4u, (uint32_t)ok); word_off++;
    store_u32be_ptr(out + word_off * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS)); word_off++;
    store_u32be_ptr(out + word_off * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK)); word_off++;
    store_u32be_ptr(out + word_off * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS)); word_off++;

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
