#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t u8;

void GXColor4u8(u8 r, u8 g, u8 b, u8 a);
extern u32 gc_gx_color4u8_last;

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline void seed_state(u32 s) { gc_gx_color4u8_last = s ^ 0xCAFEBABEu; }
static inline u32 hash_state(void) { return rotl1(gc_gx_color4u8_last ^ 0x2468ACE0u); }
static inline void dump_state(uint8_t *out, u32 *off) { wr32be(out + (*off) * 4u, gc_gx_color4u8_last); (*off)++; }

static u32 rng_state;
static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

enum { L0_CASES = 8, L1_CASES = 16, L2_CASES = 4, L3_CASES = 2048, L4_CASES = 4, L5_CASES = 4 };

const char *gc_scenario_label(void) { return "GXColor4u8/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_color_4u8_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x4C);
    if (!out) die("gc_ram_ptr failed");

    u32 word_off = 0u;
    u32 total_cases = 0u;
    u32 master_hash = 0u;
    u32 l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    seed_state(0u); GXColor4u8(0, 0, 0, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(1u); GXColor4u8(255, 0, 0, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(2u); GXColor4u8(0, 255, 0, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(3u); GXColor4u8(0, 0, 255, 0); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(4u); GXColor4u8(0, 0, 0, 255); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(5u); GXColor4u8(0x12, 0x34, 0x56, 0x78); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(6u); GXColor4u8(0xAB, 0xCD, 0xEF, 0x01); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(7u); GXColor4u8(0xFF, 0xFF, 0xFF, 0xFF); l0_hash = rotl1(l0_hash) ^ hash_state();
    total_cases += L0_CASES; master_hash = rotl1(master_hash) ^ l0_hash;

    for (u32 i = 0; i < 4u; i++) {
        for (u32 j = 0; j < 4u; j++) {
            seed_state(0x100u + (i << 4) + j);
            GXColor4u8((u8)(i * 85u), (u8)(j * 85u), (u8)(i * 17u + j * 31u), (u8)(255u - i * 33u - j * 11u));
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES; master_hash = rotl1(master_hash) ^ l1_hash;

    for (u32 i = 0; i < 4u; i++) {
        seed_state(0x200u + i);
        u8 r = (u8)(0x11u * (i + 1u));
        u8 g = (u8)(0x22u * (i + 1u));
        u8 b = (u8)(0x33u * (i + 1u));
        u8 a = (u8)(0x44u * (i + 1u));
        GXColor4u8(r, g, b, a);
        GXColor4u8(r, g, b, a);
        l2_hash = rotl1(l2_hash) ^ hash_state();
    }
    total_cases += L2_CASES; master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0xDEADBEEFu;
    for (u32 i = 0; i < L3_CASES; i++) {
        seed_state(rng_next());
        GXColor4u8((u8)rng_next(), (u8)rng_next(), (u8)rng_next(), (u8)rng_next());
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES; master_hash = rotl1(master_hash) ^ l3_hash;

    seed_state(0u);
    GXColor4u8(0x10, 0x20, 0x30, 0x40); l4_hash = rotl1(l4_hash) ^ hash_state();
    GXColor4u8(0x7F, 0x80, 0x81, 0x82); l4_hash = rotl1(l4_hash) ^ hash_state();
    GXColor4u8(0xAA, 0x55, 0xAA, 0x55); l4_hash = rotl1(l4_hash) ^ hash_state();
    GXColor4u8(0x00, 0x11, 0x22, 0x33); l4_hash = rotl1(l4_hash) ^ hash_state();
    total_cases += L4_CASES; master_hash = rotl1(master_hash) ^ l4_hash;

    seed_state(0x33333333u); GXColor4u8(0x00, 0x00, 0x00, 0xFF); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x44444444u); GXColor4u8(0xFF, 0x00, 0x00, 0x00); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x55555555u); GXColor4u8(0x00, 0xFF, 0x00, 0x00); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x66666666u); GXColor4u8(0x00, 0x00, 0xFF, 0x00); l5_hash = rotl1(l5_hash) ^ hash_state();
    total_cases += L5_CASES; master_hash = rotl1(master_hash) ^ l5_hash;

    wr32be(out + word_off * 4u, 0x47534334u); word_off++;
    wr32be(out + word_off * 4u, total_cases); word_off++;
    wr32be(out + word_off * 4u, master_hash); word_off++;
    wr32be(out + word_off * 4u, l0_hash); word_off++;
    wr32be(out + word_off * 4u, L0_CASES); word_off++;
    wr32be(out + word_off * 4u, l1_hash); word_off++;
    wr32be(out + word_off * 4u, L1_CASES); word_off++;
    wr32be(out + word_off * 4u, l2_hash); word_off++;
    wr32be(out + word_off * 4u, L2_CASES); word_off++;
    wr32be(out + word_off * 4u, l3_hash); word_off++;
    wr32be(out + word_off * 4u, L3_CASES); word_off++;
    wr32be(out + word_off * 4u, l4_hash); word_off++;
    wr32be(out + word_off * 4u, L4_CASES); word_off++;
    wr32be(out + word_off * 4u, l5_hash); word_off++;
    wr32be(out + word_off * 4u, L5_CASES); word_off++;

    seed_state(0u); GXColor4u8(0x10, 0x20, 0x30, 0x40); dump_state(out, &word_off);
    seed_state(0u); GXColor4u8(0xAB, 0xCD, 0xEF, 0x01); dump_state(out, &word_off);
    seed_state(0u); GXColor4u8(0x00, 0x00, 0xFF, 0x80); dump_state(out, &word_off);
    seed_state(0u); GXColor4u8(0xFF, 0x80, 0x00, 0x7F); dump_state(out, &word_off);
}
