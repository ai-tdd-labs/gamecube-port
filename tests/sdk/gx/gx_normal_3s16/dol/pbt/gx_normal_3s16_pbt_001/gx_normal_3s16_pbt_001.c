#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef int16_t s16;

void GXNormal3s16(s16 x, s16 y, s16 z);
extern u32 gc_gx_normal3s16_x;
extern u32 gc_gx_normal3s16_y;
extern u32 gc_gx_normal3s16_z;

static inline void store_u32be_ptr(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline void seed_state(u32 s) {
    gc_gx_normal3s16_x = s ^ 0x11111111u;
    gc_gx_normal3s16_y = s ^ 0x22222222u;
    gc_gx_normal3s16_z = s ^ 0x33333333u;
}

static inline u32 hash_state(void) {
    u32 h = 0u;
    h = rotl1(h) ^ gc_gx_normal3s16_x;
    h = rotl1(h) ^ gc_gx_normal3s16_y;
    h = rotl1(h) ^ gc_gx_normal3s16_z;
    return h;
}

static inline void dump_state(volatile uint8_t *out, u32 *off) {
    store_u32be_ptr(out + (*off) * 4u, gc_gx_normal3s16_x); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_normal3s16_y); (*off)++;
    store_u32be_ptr(out + (*off) * 4u, gc_gx_normal3s16_z); (*off)++;
}

static u32 rng_state;
static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

enum { L0_CASES = 8, L1_CASES = 16, L2_CASES = 4, L3_CASES = 2048, L4_CASES = 4, L5_CASES = 4 };

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 word_off = 0u;
    u32 total_cases = 0u;
    u32 master_hash = 0u;
    u32 l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    seed_state(0u); GXNormal3s16(0, 0, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(1u); GXNormal3s16(1, -1, 2); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(2u); GXNormal3s16(32767, -32768, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(3u); GXNormal3s16(-1024, 1023, -512); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(4u); GXNormal3s16(0x7FFF, 0x7FFF, 0x7FFF); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(5u); GXNormal3s16((s16)0x8000, (s16)0x8000, (s16)0x8000); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(6u); GXNormal3s16((s16)0x00FF, (s16)0xFF00, (s16)0x0F0F); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(7u); GXNormal3s16((s16)0xFFFF, (s16)0x0001, (s16)0x8001); l0_hash = rotl1(l0_hash) ^ hash_state();
    total_cases += L0_CASES; master_hash = rotl1(master_hash) ^ l0_hash;

    for (u32 i = 0; i < 4u; i++) {
        for (u32 j = 0; j < 4u; j++) {
            seed_state(0x100u + (i << 4) + j);
            GXNormal3s16((s16)(i * 3000 - (s16)(j * 500)),
                         (s16)(j * 2500 - (s16)(i * 700)),
                         (s16)(i * 1111 + j * 2222));
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES; master_hash = rotl1(master_hash) ^ l1_hash;

    for (u32 i = 0; i < 4u; i++) {
        seed_state(0x200u + i);
        s16 x = (s16)(0x1111u * (i + 1u));
        s16 y = (s16)(0x2222u * (i + 1u));
        s16 z = (s16)(0x3333u * (i + 1u));
        GXNormal3s16(x, y, z);
        GXNormal3s16(x, y, z);
        l2_hash = rotl1(l2_hash) ^ hash_state();
    }
    total_cases += L2_CASES; master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0x87654321u;
    for (u32 i = 0; i < L3_CASES; i++) {
        seed_state(rng_next());
        GXNormal3s16((s16)rng_next(), (s16)rng_next(), (s16)rng_next());
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES; master_hash = rotl1(master_hash) ^ l3_hash;

    seed_state(0u);
    GXNormal3s16(123, -456, 789); l4_hash = rotl1(l4_hash) ^ hash_state();
    GXNormal3s16(-321, 654, -987); l4_hash = rotl1(l4_hash) ^ hash_state();
    GXNormal3s16((s16)0x7FFF, 0, (s16)0x8000); l4_hash = rotl1(l4_hash) ^ hash_state();
    GXNormal3s16((s16)0x00FF, (s16)0xFF00, (s16)0x0F0F); l4_hash = rotl1(l4_hash) ^ hash_state();
    total_cases += L4_CASES; master_hash = rotl1(master_hash) ^ l4_hash;

    seed_state(0x33333333u); GXNormal3s16(0, -1, 1); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x44444444u); GXNormal3s16((s16)0x7FFF, (s16)0x8000, (s16)0xFFFF); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x55555555u); GXNormal3s16((s16)0x00FF, (s16)0xFF00, (s16)0xFF80); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x66666666u); GXNormal3s16((s16)0x0100, (s16)0x8001, (s16)0x7F00); l5_hash = rotl1(l5_hash) ^ hash_state();
    total_cases += L5_CASES; master_hash = rotl1(master_hash) ^ l5_hash;

    store_u32be_ptr(out + word_off * 4u, 0x47534E33u); word_off++; // GSN3
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

    seed_state(0u); GXNormal3s16(123, -456, 789); dump_state(out, &word_off);
    seed_state(0u); GXNormal3s16(-321, 654, -987); dump_state(out, &word_off);
    seed_state(0u); GXNormal3s16((s16)0x00FF, (s16)0xFF00, (s16)0x0F0F); dump_state(out, &word_off);
    seed_state(0u); GXNormal3s16((s16)0x7FFF, (s16)0x8000, (s16)0xFFFF); dump_state(out, &word_off);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
