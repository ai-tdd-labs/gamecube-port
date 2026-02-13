#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef uint16_t u16;

void GXColor1x16(u16 index);
extern u32 gc_gx_color1x16_last;

static inline void store_u32be_ptr(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline u32 rotl1(u32 v) {
    return (v << 1) | (v >> 31);
}

static inline void seed_state(u32 s) {
    gc_gx_color1x16_last = s ^ 0xA5A55A5Au;
}

static inline u32 hash_state(void) {
    return rotl1(gc_gx_color1x16_last ^ 0x13579BDFu);
}

static inline void dump_state(volatile uint8_t *out, u32 *off) {
    store_u32be_ptr(out + (*off) * 4u, gc_gx_color1x16_last); (*off)++;
}

static u32 rng_state;

static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

enum {
    L0_CASES = 8,
    L1_CASES = 16,
    L2_CASES = 4,
    L3_CASES = 2048,
    L4_CASES = 4,
    L5_CASES = 4,
};

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 word_off = 0u;
    u32 total_cases = 0u;
    u32 master_hash = 0u;
    u32 l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    seed_state(0u); GXColor1x16(0x0000u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(1u); GXColor1x16(0x0001u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(2u); GXColor1x16(0x00FFu); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(3u); GXColor1x16(0x0100u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(4u); GXColor1x16(0x7FFFu); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(5u); GXColor1x16(0x8000u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(6u); GXColor1x16(0xFF00u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(7u); GXColor1x16(0xFFFFu); l0_hash = rotl1(l0_hash) ^ hash_state();
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    for (u32 i = 0; i < 4u; i++) {
        for (u32 j = 0; j < 4u; j++) {
            seed_state(0x100u + (i << 4) + j);
            GXColor1x16((u16)(((i * 0x1111u) ^ (j * 0x0101u)) & 0xFFFFu));
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    for (u32 i = 0; i < 4u; i++) {
        seed_state(0x200u + i);
        u16 v = (u16)(0x1234u * (i + 1u));
        GXColor1x16(v);
        GXColor1x16(v);
        l2_hash = rotl1(l2_hash) ^ hash_state();
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0xC0FFEE11u;
    for (u32 i = 0; i < L3_CASES; i++) {
        seed_state(rng_next());
        GXColor1x16((u16)rng_next());
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    seed_state(0u);
    GXColor1x16(0x1234u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXColor1x16(0xABCDu);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXColor1x16(0x00F0u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXColor1x16(0x0F00u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    seed_state(0x33333333u); GXColor1x16(0x0000u); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x44444444u); GXColor1x16(0xFFFFu); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x55555555u); GXColor1x16(0x00FFu); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x66666666u); GXColor1x16(0xFF00u); l5_hash = rotl1(l5_hash) ^ hash_state();
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    store_u32be_ptr(out + word_off * 4u, 0x47534331u); word_off++; // GSC1
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

    seed_state(0u); GXColor1x16(0x1234u); dump_state(out, &word_off);
    seed_state(0u); GXColor1x16(0xABCDu); dump_state(out, &word_off);
    seed_state(0u); GXColor1x16(0x00FFu); dump_state(out, &word_off);
    seed_state(0u); GXColor1x16(0xFF00u); dump_state(out, &word_off);

    while (1) {
        __asm__ volatile("nop");
    }
    return 0;
}
