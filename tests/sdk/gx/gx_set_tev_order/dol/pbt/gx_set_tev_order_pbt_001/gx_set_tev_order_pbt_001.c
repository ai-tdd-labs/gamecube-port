/*
 * GXSetTevOrder exhaustive DOL PBT v2
 *
 * Six test levels:
 *   L0: Isolated      — reset, single call, hash (14688 exhaustive)
 *   L1: Accumulation  — even+odd stage pair on same tref entry (8000 random)
 *   L2: Overwrite     — same stage called twice, second must win (1000 random)
 *   L3: Random start  — non-zero initial state, check set_field precision (1000 random)
 *   L4: Harvest replay — 15 real game calls in sequence
 *   L5: Boundary      — stage=16 noop, max valid stage, edge values
 */
#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

/* ================================================================
 * Self-contained GXSetTevOrder (matches sdk_port/gx/GX.c:2758)
 * ================================================================ */

static u32 s_tref[8];
static u32 s_texmap_id[16];
static u32 s_tev_tc_enab;
static u32 s_bp_sent_not;
static u32 s_dirty_state;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color) {
    static const int c2r[] = { 0, 1, 0, 1, 0, 1, 7, 5, 6 };
    if (stage >= 16) return;

    u32 *ptref = &s_tref[stage / 2];
    s_texmap_id[stage] = map;

    u32 tmap = map & ~0x100u;
    if (tmap >= 8u) tmap = 0;

    u32 tcoord;
    if (coord >= 8u) {
        tcoord = 0;
        s_tev_tc_enab &= ~(1u << stage);
    } else {
        tcoord = coord;
        s_tev_tc_enab |= (1u << stage);
    }

    u32 rasc = (color == 0xFFu) ? 7u : (u32)c2r[color];
    u32 enable = (map != 0xFFu) && ((map & 0x100u) == 0);

    if (stage & 1u) {
        *ptref = set_field(*ptref, 3, 12, tmap);
        *ptref = set_field(*ptref, 3, 15, tcoord);
        *ptref = set_field(*ptref, 3, 19, rasc);
        *ptref = set_field(*ptref, 1, 18, enable);
    } else {
        *ptref = set_field(*ptref, 3, 0, tmap);
        *ptref = set_field(*ptref, 3, 3, tcoord);
        *ptref = set_field(*ptref, 3, 7, rasc);
        *ptref = set_field(*ptref, 1, 6, enable);
    }

    s_bp_sent_not = 0;
    s_dirty_state |= 1u;
}

/* ================================================================
 * Helpers
 * ================================================================ */

static void reset_state(void) {
    int i;
    for (i = 0; i < 8; i++)  s_tref[i] = 0;
    for (i = 0; i < 16; i++) s_texmap_id[i] = 0;
    s_tev_tc_enab = 0;
    s_bp_sent_not = 1;
    s_dirty_state = 0;
}

static inline void wr32be(volatile u8 *p, u32 v) {
    p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);  p[3] = (u8)(v >> 0);
}

static u32 hash_state(void) {
    u32 h = 0;
    int i;
    for (i = 0; i < 8; i++)
        h = (h << 1 | h >> 31) ^ s_tref[i];
    for (i = 0; i < 16; i++)
        h = (h << 1 | h >> 31) ^ s_texmap_id[i];
    h = (h << 1 | h >> 31) ^ s_tev_tc_enab;
    h = (h << 1 | h >> 31) ^ s_bp_sent_not;
    h = (h << 1 | h >> 31) ^ s_dirty_state;
    return h;
}

static void dump_full_state(volatile u8 *p, u32 *off) {
    int i;
    for (i = 0; i < 8; i++)  { wr32be(p + *off, s_tref[i]);      *off += 4; }
    for (i = 0; i < 16; i++) { wr32be(p + *off, s_texmap_id[i]);  *off += 4; }
    wr32be(p + *off, s_tev_tc_enab); *off += 4;
    wr32be(p + *off, s_bp_sent_not); *off += 4;
    wr32be(p + *off, s_dirty_state); *off += 4;
}

/* ---- PRNG (xorshift32, fixed seed for reproducibility) ---- */

static u32 rng_state;

static u32 rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* ---- Valid input arrays ---- */

static const u32 coords[9]  = {0,1,2,3,4,5,6,7, 0xFF};
static const u32 maps[17]   = {0,1,2,3,4,5,6,7,
                                0x100,0x101,0x102,0x103,0x104,0x105,0x106,0x107,
                                0xFF};
static const u32 colors[6]  = {4,5,6,7,8, 0xFF};

#define N_STAGES  16
#define N_COORDS   9
#define N_MAPS    17
#define N_COLORS   6

/* ---- Harvest data: 15 real game calls from MP4 boot ---- */

static const u32 harvest_stage[15] = {0,1,2,3,4,5,6,7, 8,9,10,11,12,13,14};
static const u32 harvest_coord[15] = {0,1,2,3,4,5,6,7, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const u32 harvest_map[15]   = {0,1,2,3,4,5,6,7, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const u32 harvest_color[15] = {4,4,4,4,4,4,4,4, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/* ================================================================
 * Output layout at 0x80300000, size 0x200 (512 bytes)
 *
 *  [0x000] u32 magic = 0xBEEF0002
 *  [0x004] u32 total_cases
 *  [0x008] u32 master_hash
 *
 *  [0x00C] u32 l0_hash          L0: Isolated
 *  [0x010] u32 l0_cases
 *
 *  [0x014] u32 l1_hash          L1: Accumulation
 *  [0x018] u32 l1_cases
 *
 *  [0x01C] u32 l2_hash          L2: Overwrite
 *  [0x020] u32 l2_cases
 *
 *  [0x024] u32 l3_hash          L3: Random start
 *  [0x028] u32 l3_cases
 *
 *  [0x02C] u32 l4_hash          L4: Harvest replay
 *
 *  [0x030] u32 l5_hash          L5: Boundary
 *  [0x034] u32 l5_cases
 *
 *  [0x038] L0 spot: full state after (0,0,0,4)       108 bytes → 0x0A4
 *  [0x0A4] L1 spot: full state after stages 0+1      108 bytes → 0x110
 *  [0x110] L4 spot: full state after 15 harvest calls 108 bytes → 0x17C
 *
 *  Total used: 0x17C = 380 bytes (fits in 0x200)
 * ================================================================ */

#define OUT_SIZE 0x200

int main(void) {
    volatile u8 *out = (volatile u8 *)0x80300000;
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
     * L1: Accumulation — even+odd stage pairs share one tref entry
     *     Tests that setting odd stage preserves even stage bits
     *     and vice versa. 1000 random combos per tref pair (8 pairs).
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
                /* Extract rng calls to avoid unspecified arg eval order */
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
     *     Tests that re-calling with new params fully replaces old.
     * ============================================================ */
    u32 l2_hash = 0;
    u32 l2_cases = 0;
    rng_state = 0xCAFEBABE;
    {
        for (i = 0; i < 1000; i++) {
            reset_state();
            u32 stage = rng_next() % N_STAGES;
            /* Extract rng calls to avoid unspecified arg eval order */
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
     * L3: Random start state — tref/texmap/tc_enab start non-zero
     *     Tests that set_field only touches target bits.
     * ============================================================ */
    u32 l3_hash = 0;
    u32 l3_cases = 0;
    rng_state = 0x13377331;
    {
        for (i = 0; i < 1000; i++) {
            /* Fill state with random garbage */
            int j;
            for (j = 0; j < 8; j++)  s_tref[j] = rng_next();
            for (j = 0; j < 16; j++) s_texmap_id[j] = rng_next();
            s_tev_tc_enab = rng_next();
            s_bp_sent_not = rng_next() & 1;
            s_dirty_state = rng_next();

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
     *     Replays the exact MP4 boot TEV setup.
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
     * L5: Boundary — edge cases and noop verification
     * ============================================================ */
    u32 l5_hash = 0;
    u32 l5_cases = 0;
    {
        /* stage=16 must be a noop */
        reset_state();
        u32 h_before = hash_state();
        GXSetTevOrder(16, 0, 0, 4);
        u32 h_after = hash_state();
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_before;
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_after;
        l5_cases++;

        /* stage=17 must be a noop */
        reset_state();
        h_before = hash_state();
        GXSetTevOrder(17, 0, 0, 4);
        h_after = hash_state();
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_before;
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_after;
        l5_cases++;

        /* stage=0xFFFFFFFF must be a noop */
        reset_state();
        h_before = hash_state();
        GXSetTevOrder(0xFFFFFFFFu, 0, 0, 4);
        h_after = hash_state();
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_before;
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ h_after;
        l5_cases++;

        /* stage=15 (max valid) with extreme params */
        reset_state();
        GXSetTevOrder(15, 0xFF, 0xFF, 0xFF);
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ hash_state();
        l5_cases++;

        /* stage=0 with all disabled */
        reset_state();
        GXSetTevOrder(0, 0xFF, 0xFF, 0xFF);
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ hash_state();
        l5_cases++;

        /* stage=0 with indirect map (0x100) */
        reset_state();
        GXSetTevOrder(0, 0, 0x100, 4);
        l5_hash = (l5_hash << 1 | l5_hash >> 31) ^ hash_state();
        l5_cases++;

        /* stage=0 with max valid indirect map (0x107) */
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
    wr32be(out + off, 0xBEEF0002u); off += 4;  /* magic v2 */
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
    /* off = 0x038 */

    /* Spot check L0: stage=0, coord=0, map=0, color=4 */
    reset_state();
    GXSetTevOrder(0, 0, 0, 4);
    dump_full_state(out, &off);
    /* off = 0x0A4 */

    /* Spot check L1: stages 0+1 accumulated */
    reset_state();
    GXSetTevOrder(0, 2, 3, 5);
    GXSetTevOrder(1, 4, 1, 7);
    dump_full_state(out, &off);
    /* off = 0x110 */

    /* Spot check L4: full harvest replay state */
    reset_state();
    for (i = 0; i < 15; i++) {
        GXSetTevOrder(harvest_stage[i], harvest_coord[i],
                      harvest_map[i], harvest_color[i]);
    }
    dump_full_state(out, &off);
    /* off = 0x17C */

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
