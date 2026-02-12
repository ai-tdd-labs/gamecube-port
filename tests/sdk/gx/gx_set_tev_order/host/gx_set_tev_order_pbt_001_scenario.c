/*
 * GXSetTevOrder exhaustive PBT v2 — host scenario
 *
 * Must produce byte-identical output to the DOL at 0x80300000.
 * Six test levels: L0-L5 (see DOL source for details).
 */
#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t  u8;

/* Port globals */
extern u32 gc_gx_tref[8];
extern u32 gc_gx_texmap_id[16];
extern u32 gc_gx_tev_tc_enab;
extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_dirty_state;

void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color);

/* ---- Input arrays (MUST match DOL exactly) ---- */

static const u32 coords[9]  = {0,1,2,3,4,5,6,7, 0xFF};
static const u32 maps[17]   = {0,1,2,3,4,5,6,7,
                                0x100,0x101,0x102,0x103,0x104,0x105,0x106,0x107,
                                0xFF};
static const u32 colors[6]  = {4,5,6,7,8, 0xFF};

#define N_STAGES  16
#define N_COORDS   9
#define N_MAPS    17
#define N_COLORS   6

static const u32 harvest_stage[15] = {0,1,2,3,4,5,6,7, 8,9,10,11,12,13,14};
static const u32 harvest_coord[15] = {0,1,2,3,4,5,6,7, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const u32 harvest_map[15]   = {0,1,2,3,4,5,6,7, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const u32 harvest_color[15] = {4,4,4,4,4,4,4,4, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* ---- Helpers ---- */

static void reset_state(void) {
    int i;
    for (i = 0; i < 8; i++)  gc_gx_tref[i] = 0;
    for (i = 0; i < 16; i++) gc_gx_texmap_id[i] = 0;
    gc_gx_tev_tc_enab = 0;
    gc_gx_bp_sent_not = 1;
    gc_gx_dirty_state = 0;
}

static u32 hash_state(void) {
    u32 h = 0;
    int i;
    for (i = 0; i < 8; i++)
        h = (h << 1 | h >> 31) ^ gc_gx_tref[i];
    for (i = 0; i < 16; i++)
        h = (h << 1 | h >> 31) ^ gc_gx_texmap_id[i];
    h = (h << 1 | h >> 31) ^ gc_gx_tev_tc_enab;
    h = (h << 1 | h >> 31) ^ gc_gx_bp_sent_not;
    h = (h << 1 | h >> 31) ^ gc_gx_dirty_state;
    return h;
}

static void dump_full_state(u8 *p, u32 *off) {
    int i;
    for (i = 0; i < 8; i++)  { wr32be(p + *off, gc_gx_tref[i]);      *off += 4; }
    for (i = 0; i < 16; i++) { wr32be(p + *off, gc_gx_texmap_id[i]);  *off += 4; }
    wr32be(p + *off, gc_gx_tev_tc_enab); *off += 4;
    wr32be(p + *off, gc_gx_bp_sent_not); *off += 4;
    wr32be(p + *off, gc_gx_dirty_state); *off += 4;
}

/* ---- PRNG (MUST match DOL exactly) ---- */

static u32 rng_state;

static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* ---- Scenario ---- */

#define OUT_SIZE 0x200

const char *gc_scenario_label(void) { return "GXSetTevOrder/pbt_v2"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_order_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    u8 *out = gc_ram_ptr(ram, 0x80300000u, OUT_SIZE);
    if (!out) die("gc_ram_ptr failed");

    u32 i;
    for (i = 0; i < OUT_SIZE; i++) out[i] = 0;

    u32 master_hash = 0;
    u32 total_cases = 0;

    /* ============================================================
     * L0: Isolated — exhaustive single-call test (14688 cases)
     * ============================================================ */
    u32 l0_hash = 0;
    u32 l0_cases = 0;
    {
        u32 si, ci, mi, ki;
        for (si = 0; si < N_STAGES; si++) {
            for (ci = 0; ci < N_COORDS; ci++) {
                for (mi = 0; mi < N_MAPS; mi++) {
                    for (ki = 0; ki < N_COLORS; ki++) {
                        reset_state();
                        GXSetTevOrder(si, coords[ci], maps[mi], colors[ki]);
                        u32 h = hash_state();
                        l0_hash = (l0_hash << 1 | l0_hash >> 31) ^ h;
                        l0_cases++;
                    }
                }
            }
        }
    }
    master_hash = (master_hash << 1 | master_hash >> 31) ^ l0_hash;
    total_cases += l0_cases;

    /* ============================================================
     * L1: Accumulation — even+odd stage pairs
     * ============================================================ */
    u32 l1_hash = 0;
    u32 l1_cases = 0;
    rng_state = 0xDEADBEEF;
    {
        u32 pair;
        for (pair = 0; pair < 8; pair++) {
            u32 even_stage = pair * 2;
            u32 odd_stage  = pair * 2 + 1;
            for (i = 0; i < 1000; i++) {
                reset_state();
                u32 ec = rng_next() % N_COORDS;
                u32 em = rng_next() % N_MAPS;
                u32 ek = rng_next() % N_COLORS;
                GXSetTevOrder(even_stage, coords[ec], maps[em], colors[ek]);
                u32 oc = rng_next() % N_COORDS;
                u32 om = rng_next() % N_MAPS;
                u32 ok = rng_next() % N_COLORS;
                GXSetTevOrder(odd_stage, coords[oc], maps[om], colors[ok]);
                u32 h = hash_state();
                l1_hash = (l1_hash << 1 | l1_hash >> 31) ^ h;
                l1_cases++;
            }
        }
    }
    master_hash = (master_hash << 1 | master_hash >> 31) ^ l1_hash;
    total_cases += l1_cases;

    /* ============================================================
     * L2: Overwrite — call same stage twice, second must win
     * ============================================================ */
    u32 l2_hash = 0;
    u32 l2_cases = 0;
    rng_state = 0xCAFEBABE;
    {
        for (i = 0; i < 1000; i++) {
            reset_state();
            u32 stage = rng_next() % N_STAGES;
            u32 c1 = rng_next() % N_COORDS;
            u32 m1 = rng_next() % N_MAPS;
            u32 k1 = rng_next() % N_COLORS;
            GXSetTevOrder(stage, coords[c1], maps[m1], colors[k1]);
            u32 c2 = rng_next() % N_COORDS;
            u32 m2 = rng_next() % N_MAPS;
            u32 k2 = rng_next() % N_COLORS;
            GXSetTevOrder(stage, coords[c2], maps[m2], colors[k2]);
            u32 h = hash_state();
            l2_hash = (l2_hash << 1 | l2_hash >> 31) ^ h;
            l2_cases++;
        }
    }
    master_hash = (master_hash << 1 | master_hash >> 31) ^ l2_hash;
    total_cases += l2_cases;

    /* ============================================================
     * L3: Random start state
     * ============================================================ */
    u32 l3_hash = 0;
    u32 l3_cases = 0;
    rng_state = 0x13377331;
    {
        for (i = 0; i < 1000; i++) {
            int j;
            for (j = 0; j < 8; j++)  gc_gx_tref[j] = rng_next();
            for (j = 0; j < 16; j++) gc_gx_texmap_id[j] = rng_next();
            gc_gx_tev_tc_enab = rng_next();
            gc_gx_bp_sent_not = rng_next() & 1;
            gc_gx_dirty_state = rng_next();

            u32 stage = rng_next() % N_STAGES;
            u32 rc = rng_next() % N_COORDS;
            u32 rm = rng_next() % N_MAPS;
            u32 rk = rng_next() % N_COLORS;
            GXSetTevOrder(stage, coords[rc], maps[rm], colors[rk]);
            u32 h = hash_state();
            l3_hash = (l3_hash << 1 | l3_hash >> 31) ^ h;
            l3_cases++;
        }
    }
    master_hash = (master_hash << 1 | master_hash >> 31) ^ l3_hash;
    total_cases += l3_cases;

    /* ============================================================
     * L4: Harvest replay — 15 real game calls in sequence
     * ============================================================ */
    u32 l4_hash;
    reset_state();
    {
        for (i = 0; i < 15; i++) {
            GXSetTevOrder(harvest_stage[i], harvest_coord[i],
                          harvest_map[i], harvest_color[i]);
        }
        l4_hash = hash_state();
    }
    master_hash = (master_hash << 1 | master_hash >> 31) ^ l4_hash;
    total_cases += 15;

    /* ============================================================
     * L5: Boundary
     * ============================================================ */
    u32 l5_hash = 0;
    u32 l5_cases = 0;
    {
        reset_state();
        u32 h_before = hash_state();
        GXSetTevOrder(16, 0, 0, 4);
        u32 h_after = hash_state();
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_before;
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_after;
        l5_cases++;

        reset_state();
        h_before = hash_state();
        GXSetTevOrder(17, 0, 0, 4);
        h_after = hash_state();
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_before;
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_after;
        l5_cases++;

        reset_state();
        h_before = hash_state();
        GXSetTevOrder(0xFFFFFFFFu, 0, 0, 4);
        h_after = hash_state();
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_before;
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_after;
        l5_cases++;

        reset_state();
        GXSetTevOrder(15, 0xFF, 0xFF, 0xFF);
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ hash_state();
        l5_cases++;

        reset_state();
        GXSetTevOrder(0, 0xFF, 0xFF, 0xFF);
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ hash_state();
        l5_cases++;

        reset_state();
        GXSetTevOrder(0, 0, 0x100, 4);
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ hash_state();
        l5_cases++;

        reset_state();
        GXSetTevOrder(0, 7, 0x107, 8);
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ hash_state();
        l5_cases++;
    }
    master_hash = (master_hash << 1 | master_hash >> 31) ^ l5_hash;
    total_cases += l5_cases;

    /* ================================================================
     * Write output
     * ================================================================ */
    u32 off = 0;
    wr32be(out + off, 0xBEEF0002u); off += 4;
    wr32be(out + off, total_cases);  off += 4;
    wr32be(out + off, master_hash);  off += 4;

    wr32be(out + off, l0_hash);  off += 4;
    wr32be(out + off, l0_cases); off += 4;

    wr32be(out + off, l1_hash);  off += 4;
    wr32be(out + off, l1_cases); off += 4;

    wr32be(out + off, l2_hash);  off += 4;
    wr32be(out + off, l2_cases); off += 4;

    wr32be(out + off, l3_hash);  off += 4;
    wr32be(out + off, l3_cases); off += 4;

    wr32be(out + off, l4_hash);  off += 4;

    wr32be(out + off, l5_hash);  off += 4;
    wr32be(out + off, l5_cases); off += 4;

    /* Spot check L0 */
    reset_state();
    GXSetTevOrder(0, 0, 0, 4);
    dump_full_state(out, &off);

    /* Spot check L1 */
    reset_state();
    GXSetTevOrder(0, 2, 3, 5);
    GXSetTevOrder(1, 4, 1, 7);
    dump_full_state(out, &off);

    /* Spot check L4 */
    reset_state();
    for (i = 0; i < 15; i++) {
        GXSetTevOrder(harvest_stage[i], harvest_coord[i],
                      harvest_map[i], harvest_color[i]);
    }
    dump_full_state(out, &off);
}
