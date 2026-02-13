#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

void GXTexCoord1x16(u16 index);
extern u32 gc_gx_texcoord1x16_last;

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline void seed_state(u32 s) { gc_gx_texcoord1x16_last = s ^ 0x5AA5A55Au; }
static inline u32 hash_state(void) { return rotl1(gc_gx_texcoord1x16_last ^ 0x89ABCDEFu); }
static inline void dump_state(uint8_t *out, u32 *off) { wr32be(out + (*off) * 4u, gc_gx_texcoord1x16_last); (*off)++; }

static u32 rng_state;
static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

enum { L0_CASES = 8, L1_CASES = 16, L2_CASES = 4, L3_CASES = 2048, L4_CASES = 4, L5_CASES = 4 };

const char *gc_scenario_label(void) { return "GXTexCoord1x16/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_tex_coord_1x16_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x4C);
    if (!out) die("gc_ram_ptr failed");

    u32 word_off = 0u;
    u32 total_cases = 0u;
    u32 master_hash = 0u;
    u32 l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    seed_state(0u); GXTexCoord1x16(0x0000u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(1u); GXTexCoord1x16(0x0001u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(2u); GXTexCoord1x16(0x00FFu); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(3u); GXTexCoord1x16(0x0100u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(4u); GXTexCoord1x16(0x7FFFu); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(5u); GXTexCoord1x16(0x8000u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(6u); GXTexCoord1x16(0xFF00u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(7u); GXTexCoord1x16(0xFFFFu); l0_hash = rotl1(l0_hash) ^ hash_state();
    total_cases += L0_CASES; master_hash = rotl1(master_hash) ^ l0_hash;

    for (u32 i = 0; i < 4u; i++) {
        for (u32 j = 0; j < 4u; j++) {
            seed_state(0x100u + (i << 4) + j);
            GXTexCoord1x16((u16)(((i * 0x1111u) ^ (j * 0x0101u)) & 0xFFFFu));
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES; master_hash = rotl1(master_hash) ^ l1_hash;

    for (u32 i = 0; i < 4u; i++) {
        seed_state(0x200u + i);
        u16 v = (u16)(0x4321u * (i + 1u));
        GXTexCoord1x16(v);
        GXTexCoord1x16(v);
        l2_hash = rotl1(l2_hash) ^ hash_state();
    }
    total_cases += L2_CASES; master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0x12345678u;
    for (u32 i = 0; i < L3_CASES; i++) {
        seed_state(rng_next());
        GXTexCoord1x16((u16)rng_next());
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES; master_hash = rotl1(master_hash) ^ l3_hash;

    seed_state(0u);
    GXTexCoord1x16(0x1357u); l4_hash = rotl1(l4_hash) ^ hash_state();
    GXTexCoord1x16(0x2468u); l4_hash = rotl1(l4_hash) ^ hash_state();
    GXTexCoord1x16(0xAAAAu); l4_hash = rotl1(l4_hash) ^ hash_state();
    GXTexCoord1x16(0x5555u); l4_hash = rotl1(l4_hash) ^ hash_state();
    total_cases += L4_CASES; master_hash = rotl1(master_hash) ^ l4_hash;

    seed_state(0x33333333u); GXTexCoord1x16(0x0000u); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x44444444u); GXTexCoord1x16(0xFFFFu); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x55555555u); GXTexCoord1x16(0x00FFu); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x66666666u); GXTexCoord1x16(0xFF00u); l5_hash = rotl1(l5_hash) ^ hash_state();
    total_cases += L5_CASES; master_hash = rotl1(master_hash) ^ l5_hash;

    wr32be(out + word_off * 4u, 0x47534E31u); word_off++;
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

    seed_state(0u); GXTexCoord1x16(0x1357u); dump_state(out, &word_off);
    seed_state(0u); GXTexCoord1x16(0x2468u); dump_state(out, &word_off);
    seed_state(0u); GXTexCoord1x16(0x00FFu); dump_state(out, &word_off);
    seed_state(0u); GXTexCoord1x16(0xFF00u); dump_state(out, &word_off);
}
