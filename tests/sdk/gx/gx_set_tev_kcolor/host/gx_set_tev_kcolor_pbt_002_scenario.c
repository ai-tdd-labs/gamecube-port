#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef int16_t s16;

typedef struct { s16 r, g, b, a; } GXColorS10;

void GXSetTevColorS10(u32 id, GXColorS10 color);

extern u32 gc_gx_tev_colors10_ra_last;
extern u32 gc_gx_tev_colors10_bg_last;
extern u32 gc_gx_bp_sent_not;

static inline u32 rotl1(u32 v) {
    return (v << 1) | (v >> 31);
}

static inline void seed_state(u32 s) {
    gc_gx_tev_colors10_ra_last = s ^ 0x11111111u;
    gc_gx_tev_colors10_bg_last = s ^ 0x22222222u;
    gc_gx_bp_sent_not = s ^ 0xA5A5A5A5u;
}

static inline u32 hash_state(void) {
    u32 h = 0u;
    h = rotl1(h) ^ gc_gx_tev_colors10_ra_last;
    h = rotl1(h) ^ gc_gx_tev_colors10_bg_last;
    h = rotl1(h) ^ gc_gx_bp_sent_not;
    return h;
}

static inline void dump_state(uint8_t *out, u32 *off) {
    wr32be(out + (*off) * 4u, gc_gx_tev_colors10_ra_last); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_tev_colors10_bg_last); (*off)++;
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
    L2_CASES = 6,
    L3_CASES = 1024,
    L4_CASES = 4,
    L5_CASES = 4,
};

const char *gc_scenario_label(void) { return "GXSetTevColorS10/pbt_002"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_kcolor_pbt_002.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x6C);
    if (!out) die("gc_ram_ptr failed");

    u32 word_off = 0u;
    u32 total_cases = 0u;
    u32 master_hash = 0u;
    u32 l0_hash = 0u, l1_hash = 0u, l2_hash = 0u, l3_hash = 0u, l4_hash = 0u, l5_hash = 0u;

    seed_state(0x0u); GXSetTevColorS10(0u, (GXColorS10){0, 0, 0, 0}); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x1u); GXSetTevColorS10(1u, (GXColorS10){-100, 200, -300, 400}); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x2u); GXSetTevColorS10(2u, (GXColorS10){1023, -1024, 511, -512}); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x3u); GXSetTevColorS10(3u, (GXColorS10){32767, -32768, 12345, -12345}); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x4u); GXSetTevColorS10(255u, (GXColorS10){7, 8, 9, 10}); l0_hash = rotl1(l0_hash) ^ hash_state();
    seed_state(0x5u); GXSetTevColorS10(0xFFFFFFFFu, (GXColorS10){-1, -2, -3, -4}); l0_hash = rotl1(l0_hash) ^ hash_state();
    total_cases += L0_CASES;
    master_hash = rotl1(master_hash) ^ l0_hash;

    for (u32 id = 0; id < 4u; id++) {
        for (u32 j = 0; j < 4u; j++) {
            seed_state(0x100u + (id << 4) + j);
            GXSetTevColorS10(id,
                             (GXColorS10){
                                 (s16)(id * 100 - (s16)(j * 31)),
                                 (s16)(j * 200 - (s16)(id * 47)),
                                 (s16)(500 - (s16)(id * 91 + j * 27)),
                                 (s16)(-700 + (s16)(id * 55 + j * 13))});
            l1_hash = rotl1(l1_hash) ^ hash_state();
        }
    }
    total_cases += L1_CASES;
    master_hash = rotl1(master_hash) ^ l1_hash;

    seed_state(0x200u); GXSetTevColorS10(0u, (GXColorS10){1, 2, 3, 4}); GXSetTevColorS10(0u, (GXColorS10){1, 2, 3, 4}); l2_hash = rotl1(l2_hash) ^ hash_state();
    seed_state(0x201u); GXSetTevColorS10(1u, (GXColorS10){-1, -2, -3, -4}); GXSetTevColorS10(1u, (GXColorS10){-1, -2, -3, -4}); l2_hash = rotl1(l2_hash) ^ hash_state();
    seed_state(0x202u); GXSetTevColorS10(2u, (GXColorS10){1023, 1023, 1023, 1023}); GXSetTevColorS10(2u, (GXColorS10){1023, 1023, 1023, 1023}); l2_hash = rotl1(l2_hash) ^ hash_state();
    seed_state(0x203u); GXSetTevColorS10(3u, (GXColorS10){-1024, -1024, -1024, -1024}); GXSetTevColorS10(3u, (GXColorS10){-1024, -1024, -1024, -1024}); l2_hash = rotl1(l2_hash) ^ hash_state();
    seed_state(0x204u); GXSetTevColorS10(5u, (GXColorS10){17, -17, 33, -33}); GXSetTevColorS10(5u, (GXColorS10){17, -17, 33, -33}); l2_hash = rotl1(l2_hash) ^ hash_state();
    seed_state(0x205u); GXSetTevColorS10(0x80000000u, (GXColorS10){3000, -3000, 2047, -2048}); GXSetTevColorS10(0x80000000u, (GXColorS10){3000, -3000, 2047, -2048}); l2_hash = rotl1(l2_hash) ^ hash_state();
    total_cases += L2_CASES;
    master_hash = rotl1(master_hash) ^ l2_hash;

    rng_state = 0x0BADB002u;
    for (u32 i = 0; i < L3_CASES; i++) {
        seed_state(rng_next());
        GXSetTevColorS10(rng_next(),
                         (GXColorS10){
                             (s16)rng_next(),
                             (s16)rng_next(),
                             (s16)rng_next(),
                             (s16)rng_next()});
        l3_hash = rotl1(l3_hash) ^ hash_state();
    }
    total_cases += L3_CASES;
    master_hash = rotl1(master_hash) ^ l3_hash;

    seed_state(0u);
    GXSetTevColorS10(1u, (GXColorS10){-100, 200, -300, 400});
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevColorS10(1u, (GXColorS10){-100, 200, -300, 400});
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevColorS10(0u, (GXColorS10){0x3FF, -0x400, 0x155, -0x155});
    l4_hash = rotl1(l4_hash) ^ hash_state();
    GXSetTevColorS10(3u, (GXColorS10){0x7FF, -0x800, 0x2AA, -0x2AA});
    l4_hash = rotl1(l4_hash) ^ hash_state();
    total_cases += L4_CASES;
    master_hash = rotl1(master_hash) ^ l4_hash;

    seed_state(0x33333333u); GXSetTevColorS10(0u, (GXColorS10){-32768, 32767, -32768, 32767}); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x44444444u); GXSetTevColorS10(0u, (GXColorS10){2047, -2048, 2047, -2048}); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x55555555u); GXSetTevColorS10(123456789u, (GXColorS10){-1, -1, -1, -1}); l5_hash = rotl1(l5_hash) ^ hash_state();
    seed_state(0x66666666u); GXSetTevColorS10(0u, (GXColorS10){1, -1, 1, -1}); l5_hash = rotl1(l5_hash) ^ hash_state();
    total_cases += L5_CASES;
    master_hash = rotl1(master_hash) ^ l5_hash;

    wr32be(out + word_off * 4u, 0x47535331u); word_off++;
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
    GXSetTevColorS10(1u, (GXColorS10){-100, 200, -300, 400});
    dump_state(out, &word_off);

    seed_state(0u);
    GXSetTevColorS10(0u, (GXColorS10){1023, -1024, 341, -341});
    dump_state(out, &word_off);

    seed_state(0x77777777u);
    GXSetTevColorS10(3u, (GXColorS10){32767, -32768, 12345, -12345});
    dump_state(out, &word_off);

    seed_state(0x88888888u);
    GXSetTevColorS10(0xFFFFFFFFu, (GXColorS10){-1, -2, -3, -4});
    dump_state(out, &word_off);
}
