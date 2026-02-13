#include <stdint.h>

#include "src/sdk_port/gc_mem.c"
#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;

void VISetBlack(u32 black);

static inline void store_u32be_ptr(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline u32 rotl1(u32 v) {
    return (v << 1) | (v >> 31);
}

static inline void seed_state(u32 calls, u32 black) {
    gc_sdk_state_store_u32be(GC_SDK_OFF_VI_SET_BLACK_CALLS, calls);
    gc_sdk_state_store_u32be(GC_SDK_OFF_VI_BLACK, black);
}

static inline u32 hash_state(void) {
    u32 h = 0u;
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_VI_SET_BLACK_CALLS);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_VI_BLACK);
    return h;
}

static inline void dump_state(volatile uint8_t *out, u32 *off) {
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_SET_BLACK_CALLS)); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_VI_BLACK)); (*off)++;
}

static u32 rng_state;

static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static const u32 k_inputs[] = {0u, 1u, 2u, 0xffffffffu};
static const u32 k_callsite_inputs[] = {0u, 1u, 1u, 0u};

enum {
    L0_CASES = 4,
    L1_CASES = 16,
    L2_CASES = 4,
    L3_CASES = 1024,
    L4_CASES = 4,
    L5_CASES = 2,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 word_off = 0u;
    u32 total_cases = 0u;
    u32 master_hash = 0u;
    u32 l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    // L0: isolated calls.
    for (u32 i = 0; i < 4u; i++) {
        seed_state(0u, 0u);
        VISetBlack(k_inputs[i]);
        l0_hash = rotl1(l0_hash) ^ hash_state();
    }
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    // L1: accumulation transitions.
    for (u32 i = 0; i < 4u; i++) {
        for (u32 j = 0; j < 4u; j++) {
            seed_state(0u, 0u);
            VISetBlack(k_inputs[i]);
            VISetBlack(k_inputs[j]);
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    // L2: idempotency (same value twice).
    for (u32 i = 0; i < 4u; i++) {
        seed_state(0u, 0u);
        VISetBlack(k_inputs[i]);
        VISetBlack(k_inputs[i]);
        l2_hash = rotl1(l2_hash) ^ hash_state();
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    // L3: random-start state.
    rng_state = 0x13579BDFu;
    for (u32 i = 0; i < L3_CASES; i++) {
        seed_state(rng_next(), rng_next());
        VISetBlack(k_inputs[rng_next() & 3u]);
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    // L4: callsite-derived value replay set from MP4 decomp (init/sreset values).
    seed_state(0u, 0u);
    for (u32 i = 0; i < 4u; i++) {
        VISetBlack(k_callsite_inputs[i]);
        l4_hash = rotl1(l4_hash) ^ hash_state();
    }
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    // L5: boundary/no-op invariants around boolean normalization and u32 call counter.
    seed_state(0xFFFFFFFEu, 0u);
    VISetBlack(0xFFFFFFFFu);
    l5_hash = rotl1(l5_hash) ^ hash_state();
    VISetBlack(0u);
    l5_hash = rotl1(l5_hash) ^ hash_state();
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    store_u32be_ptr(out + word_off * 4u, 0x56534250u); word_off++; // VSBP
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

    seed_state(0u, 0u);
    VISetBlack(0u);
    dump_state(out, &word_off);

    seed_state(0u, 0u);
    VISetBlack(1u);
    dump_state(out, &word_off);

    seed_state(0u, 0u);
    VISetBlack(2u);
    dump_state(out, &word_off);

    seed_state(0u, 0u);
    VISetBlack(0xFFFFFFFFu);
    dump_state(out, &word_off);

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
