#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

typedef uint32_t u32;

void PADSetSpec(u32 spec);

#define PAD_PADSPEC_SHADOW_ADDR 0x801D450Cu
#define PAD_SPEC_ADDR 0x801D3924u
#define PAD_MAKE_STATUS_ADDR 0x801D3928u

static inline uint32_t rotl1(uint32_t v) {
    return (v << 1) | (v >> 31);
}

static inline uint32_t load_u32be_addr(GcRam *ram, uint32_t addr) {
    uint8_t *p = gc_ram_ptr(ram, addr, 4u);
    if (!p) die("gc_ram_ptr failed");
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void store_u32be_addr(GcRam *ram, uint32_t addr, uint32_t v) {
    uint8_t *p = gc_ram_ptr(ram, addr, 4u);
    if (!p) die("gc_ram_ptr failed");
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline void seed_state(GcRam *ram, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_SPEC, a);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND, b);
    store_u32be_addr(ram, PAD_PADSPEC_SHADOW_ADDR, c);
    store_u32be_addr(ram, PAD_SPEC_ADDR, d);
    store_u32be_addr(ram, PAD_MAKE_STATUS_ADDR, e);
}

static inline uint32_t hash_state(GcRam *ram) {
    uint32_t h = 0u;
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_SPEC);
    h = rotl1(h) ^ gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND);
    h = rotl1(h) ^ load_u32be_addr(ram, PAD_PADSPEC_SHADOW_ADDR);
    h = rotl1(h) ^ load_u32be_addr(ram, PAD_SPEC_ADDR);
    h = rotl1(h) ^ load_u32be_addr(ram, PAD_MAKE_STATUS_ADDR);
    return h;
}

static inline void dump_state(GcRam *ram, uint8_t *out, uint32_t *off) {
    wr32be(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_SPEC)); (*off)++;
    wr32be(out + (*off) * 4u, gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND)); (*off)++;
    wr32be(out + (*off) * 4u, load_u32be_addr(ram, PAD_PADSPEC_SHADOW_ADDR)); (*off)++;
    wr32be(out + (*off) * 4u, load_u32be_addr(ram, PAD_SPEC_ADDR)); (*off)++;
    wr32be(out + (*off) * 4u, load_u32be_addr(ram, PAD_MAKE_STATUS_ADDR)); (*off)++;
}

static uint32_t rng_state;

static uint32_t rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static const uint32_t k_specs[] = {0u, 1u, 2u, 3u, 4u, 5u, 0xffffffffu};
static const uint32_t k_harvest_specs[] = {5u, 5u, 5u};

enum {
    L0_CASES = 7,
    L1_CASES = 7 * 7,
    L2_CASES = 7,
    L3_CASES = 1024,
    L4_CASES = 3,
    L5_CASES = 4,
};

const char *gc_scenario_label(void) { return "PADSetSpec/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/pad_set_spec_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x8c);
    if (!out) die("gc_ram_ptr failed");
    uint32_t word_off = 0u;
    uint32_t total_cases = 0u;
    uint32_t master_hash = 0u;
    uint32_t l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    for (uint32_t i = 0; i < 7u; i++) {
        seed_state(ram, 0u, 0u, 0u, 0u, 0u);
        PADSetSpec(k_specs[i]);
        l0_hash = rotl1(l0_hash) ^ hash_state(ram);
    }
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    for (uint32_t i = 0; i < 7u; i++) {
        for (uint32_t j = 0; j < 7u; j++) {
            seed_state(ram, 0u, 0u, 0u, 0u, 0u);
            PADSetSpec(k_specs[i]);
            PADSetSpec(k_specs[j]);
            l1_hash = rotl1(l1_hash) ^ hash_state(ram);
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    for (uint32_t i = 0; i < 7u; i++) {
        seed_state(ram, 0u, 0u, 0u, 0u, 0u);
        PADSetSpec(k_specs[i]);
        PADSetSpec(k_specs[i]);
        l2_hash = rotl1(l2_hash) ^ hash_state(ram);
    }
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0x91A2B3C4u;
    for (uint32_t i = 0; i < L3_CASES; i++) {
        seed_state(ram, rng_next(), rng_next(), rng_next(), rng_next(), rng_next());
        PADSetSpec(k_specs[rng_next() % 7u]);
        l3_hash = rotl1(l3_hash) ^ hash_state(ram);
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    seed_state(ram, 0u, 0u, 0u, 0u, 0u);
    for (uint32_t i = 0; i < 3u; i++) {
        PADSetSpec(k_harvest_specs[i]);
        l4_hash = rotl1(l4_hash) ^ hash_state(ram);
    }
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    seed_state(ram, 0xaaaaaaaau, 0xbbbbbbbbu, 0xccccccccu, 0xddddddddu, 0xeeeeeeeeu);
    PADSetSpec(0u);
    l5_hash = rotl1(l5_hash) ^ hash_state(ram);
    PADSetSpec(1u);
    l5_hash = rotl1(l5_hash) ^ hash_state(ram);
    PADSetSpec(2u);
    l5_hash = rotl1(l5_hash) ^ hash_state(ram);
    PADSetSpec(0xffffffffu);
    l5_hash = rotl1(l5_hash) ^ hash_state(ram);
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    wr32be(out + word_off * 4u, 0x50425350u); word_off++;
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

    seed_state(ram, 0u, 0u, 0u, 0u, 0u);
    PADSetSpec(5u);
    dump_state(ram, out, &word_off);

    seed_state(ram, 0u, 0u, 0u, 0u, 0u);
    PADSetSpec(0u);
    dump_state(ram, out, &word_off);

    seed_state(ram, 0u, 0u, 0u, 0u, 0u);
    PADSetSpec(1u);
    dump_state(ram, out, &word_off);

    seed_state(ram, 0u, 0u, 0u, 0u, 0u);
    PADSetSpec(0xffffffffu);
    dump_state(ram, out, &word_off);
}
