#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void GXSetTevSwapMode(u32 stage, u32 ras_sel, u32 tex_sel);
void GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha);

extern u32 gc_gx_teva[16];
extern u32 gc_gx_tev_ksel[8];
extern u32 gc_gx_bp_sent_not;

static inline u32 rotl1(u32 v) {
    return (v << 1) | (v >> 31);
}

static inline void seed_state(u32 s) {
    for (u32 i = 0; i < 16u; i++) gc_gx_teva[i] = s ^ (0x11111111u * i);
    for (u32 i = 0; i < 8u; i++) gc_gx_tev_ksel[i] = (s << (i & 7u)) ^ (0x01010101u * (i + 1u));
    gc_gx_bp_sent_not = s ^ 0xA5A5A5A5u;
}

static inline u32 hash_state(void) {
    u32 h = 0u;
    h = rotl1(h) ^ gc_gx_teva[0];
    h = rotl1(h) ^ gc_gx_teva[5];
    h = rotl1(h) ^ gc_gx_teva[15];
    h = rotl1(h) ^ gc_gx_tev_ksel[0];
    h = rotl1(h) ^ gc_gx_tev_ksel[1];
    h = rotl1(h) ^ gc_gx_tev_ksel[4];
    h = rotl1(h) ^ gc_gx_tev_ksel[5];
    h = rotl1(h) ^ gc_gx_bp_sent_not;
    return h;
}

static inline void dump_state(uint8_t *out, u32 *off) {
    wr32be(out + (*off) * 4u, gc_gx_teva[0]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_teva[5]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_teva[15]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_tev_ksel[0]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_tev_ksel[1]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_tev_ksel[4]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_tev_ksel[5]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_bp_sent_not); (*off)++;
}

static u32 rng_state;

static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

enum {
    L0_CASES = 6,
    L1_CASES = 16,
    L2_CASES = 4,
    L3_CASES = 1024,
    L4_CASES = 4,
    L5_CASES = 2,
};

const char *gc_scenario_label(void) { return "GXSetTevSwapMode/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_swap_mode_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0xBC);
    if (!out) die("gc_ram_ptr failed");

    u32 word_off = 0u;
    u32 total_cases = 0u;
    u32 master_hash = 0u;
    u32 l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    seed_state(0u); GXSetTevSwapMode(0u, 1u, 2u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(1u); GXSetTevSwapMode(5u, 3u, 0u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(2u); GXSetTevSwapMode(15u, 2u, 1u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(3u); GXSetTevSwapMode(16u, 3u, 3u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(4u); GXSetTevSwapModeTable(0u, 0u, 1u, 2u, 0u); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(5u); GXSetTevSwapModeTable(4u, 3u, 2u, 1u, 0u); l0_hash = rotl1(l0_hash) ^ hash_state();
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    for (u32 i = 0; i < 4u; i++) {
        for (u32 j = 0; j < 4u; j++) {
            seed_state(0x100u + (i << 4) + j);
            GXSetTevSwapMode(i * 5u, i, j);
            GXSetTevSwapModeTable(j, j, i, (i + j) & 3u, (i ^ j) & 3u);
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    for (u32 i = 0; i < 4u; i++) {
        seed_state(0x200u + i);
        GXSetTevSwapMode(i * 3u, i, (i + 1u) & 3u);
        GXSetTevSwapMode(i * 3u, i, (i + 1u) & 3u);
        GXSetTevSwapModeTable(i, i, i, i, i);
        GXSetTevSwapModeTable(i, i, i, i, i);
        l2_hash = rotl1(l2_hash) ^ hash_state();
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0x2468ACE1u;
    for (u32 i = 0; i < L3_CASES; i++) {
        seed_state(rng_next());
        GXSetTevSwapMode(rng_next() & 31u, rng_next(), rng_next());
        GXSetTevSwapModeTable(rng_next() & 7u, rng_next(), rng_next(), rng_next(), rng_next());
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    seed_state(0u);
    GXSetTevSwapMode(0u, 1u, 2u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevSwapMode(5u, 3u, 0u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevSwapModeTable(0u, 0u, 1u, 2u, 0u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevSwapModeTable(2u, 3u, 2u, 1u, 0u);
    l4_hash = rotl1(l4_hash) ^ hash_state();
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    seed_state(0x33333333u);
    u32 pre = hash_state();
    GXSetTevSwapMode(16u, 3u, 3u);
    l5_hash = rotl1(l5_hash) ^ (pre ^ hash_state());
    seed_state(0x44444444u);
    pre = hash_state();
    GXSetTevSwapModeTable(4u, 3u, 3u, 3u, 3u);
    l5_hash = rotl1(l5_hash) ^ (pre ^ hash_state());
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    wr32be(out + word_off * 4u, 0x47535042u); word_off++;
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

    seed_state(0u);
    GXSetTevSwapMode(0u, 1u, 2u);
    GXSetTevSwapModeTable(0u, 0u, 1u, 2u, 0u);
    dump_state(out, &word_off);

    seed_state(0u);
    GXSetTevSwapMode(5u, 3u, 0u);
    GXSetTevSwapModeTable(2u, 3u, 2u, 1u, 0u);
    dump_state(out, &word_off);

    seed_state(0u);
    GXSetTevSwapMode(15u, 2u, 1u);
    GXSetTevSwapModeTable(3u, 1u, 0u, 3u, 2u);
    dump_state(out, &word_off);

    seed_state(0x77777777u);
    GXSetTevSwapMode(16u, 3u, 3u);
    GXSetTevSwapModeTable(4u, 3u, 3u, 3u, 3u);
    dump_state(out, &word_off);
}
