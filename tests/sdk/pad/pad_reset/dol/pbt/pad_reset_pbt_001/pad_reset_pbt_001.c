#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;
typedef int BOOL;

BOOL PADReset(u32 mask);

static inline void store_u32be_ptr(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline uint32_t rotl1(uint32_t v) {
    return (v << 1) | (v >> 31);
}

static inline void seed_state(uint32_t rb, uint32_t rc, uint32_t recal, uint32_t rcb, uint32_t calls) {
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_BITS, rb);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_CHAN, rc);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, recal);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_CB_PTR, rcb);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_CALLS, calls);
}

static inline uint32_t hash_state(void) {
    uint32_t h = 0u;
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_MASK);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_CALLS);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESETTING_BITS);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESETTING_CHAN);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_CB_PTR);
    return h;
}

static inline void dump_state(volatile uint8_t *out, uint32_t *off) {
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_MASK)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_CALLS)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESETTING_BITS)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESETTING_CHAN)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_CB_PTR)); (*off)++;
}

static uint32_t rng_state;
static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static const uint32_t k_masks[] = {
    0x00000000u, 0x80000000u, 0x40000000u, 0x20000000u, 0x10000000u, 0x70000000u, 0xFFFFFFFFu,
};

enum {
    L0_CASES = 7,
    L1_CASES = 7 * 7,
    L2_CASES = 7,
    L3_CASES = 1024,
    L4_CASES = 8,
    L5_CASES = 6,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    uint32_t word_off = 0u;
    uint32_t total_cases = 0u;
    uint32_t master_hash = 0u;
    uint32_t l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    // L0: isolated masks.
    for (uint32_t i = 0; i < 7u; i++) {
        seed_state(0u, 32u, 0u, 0u, 0u);
        (void)PADReset(k_masks[i]);
        l0_hash = rotl1(l0_hash) ^ hash_state();
    }
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    // L1: accumulation sequence.
    for (uint32_t i = 0; i < 7u; i++) {
        for (uint32_t j = 0; j < 7u; j++) {
            seed_state(0u, 32u, 0u, 0u, 0u);
            (void)PADReset(k_masks[i]);
            (void)PADReset(k_masks[j]);
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    // L2: idempotency same mask twice.
    for (uint32_t i = 0; i < 7u; i++) {
        seed_state(0u, 32u, 0u, 0u, 0u);
        (void)PADReset(k_masks[i]);
        (void)PADReset(k_masks[i]);
        l2_hash = rotl1(l2_hash) ^ hash_state();
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    // L3: random-start state.
    rng_state = 0x55AA91C3u;
    for (uint32_t i = 0; i < L3_CASES; i++) {
        seed_state(rng_next(), rng_next() & 63u, rng_next(), rng_next(), rng_next());
        (void)PADReset(k_masks[rng_next() % 7u]);
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    // L4: harvest replay uses observed retail mask=0x70000000.
    seed_state(0u, 32u, 0u, 0u, 0u);
    for (uint32_t i = 0; i < L4_CASES; i++) {
        (void)PADReset(0x70000000u);
        l4_hash = rotl1(l4_hash) ^ hash_state();
    }
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    // L5: boundary from non-empty starting states.
    seed_state(0xFFFFFFFFu, 0u, 0x12345678u, 0x9ABCDEF0u, 7u);
    (void)PADReset(0u);
    l5_hash = rotl1(l5_hash) ^ hash_state();
    (void)PADReset(0x80000000u);
    l5_hash = rotl1(l5_hash) ^ hash_state();
    (void)PADReset(0x40000000u);
    l5_hash = rotl1(l5_hash) ^ hash_state();
    (void)PADReset(0x20000000u);
    l5_hash = rotl1(l5_hash) ^ hash_state();
    (void)PADReset(0x10000000u);
    l5_hash = rotl1(l5_hash) ^ hash_state();
    (void)PADReset(0xFFFFFFFFu);
    l5_hash = rotl1(l5_hash) ^ hash_state();
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    store_u32be_ptr(out + word_off * 4u, 0x50425254u); word_off++; // PBRT
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

    seed_state(0u, 32u, 0u, 0u, 0u);
    (void)PADReset(0x70000000u);
    dump_state(out, &word_off);

    seed_state(0u, 32u, 0u, 0u, 0u);
    (void)PADReset(0xA0000000u);
    dump_state(out, &word_off);

    seed_state(0xFFFFFFFFu, 0u, 1u, 2u, 3u);
    (void)PADReset(0u);
    dump_state(out, &word_off);

    seed_state(0x11111111u, 5u, 0x22222222u, 0x33333333u, 9u);
    (void)PADReset(0xFFFFFFFFu);
    dump_state(out, &word_off);

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
