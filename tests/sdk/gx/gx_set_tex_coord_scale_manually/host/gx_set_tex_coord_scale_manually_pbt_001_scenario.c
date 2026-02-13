#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

void GXSetTexCoordScaleManually(u32 coord, u8 enable, u16 ss, u16 ts);
extern u32 gc_gx_su_ts0[8];
extern u32 gc_gx_su_ts1[8];
extern u32 gc_gx_tcs_man_enab;
extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_last_ras_reg;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 8, L5 = 4 };

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline void seed_state(void) {
    for (u32 i = 0; i < 8u; i++) {
        gc_gx_su_ts0[i] = 0x70000000u ^ (i * 0x1111u);
        gc_gx_su_ts1[i] = 0x71000000u ^ (i * 0x2222u);
    }
    gc_gx_tcs_man_enab = 0;
    gc_gx_bp_sent_not = 1u;
    gc_gx_last_ras_reg = 0;
}

static inline u32 hash_state(u32 coord) {
    u32 h = 0;
    h = rotl1(h) ^ gc_gx_tcs_man_enab;
    h = rotl1(h) ^ gc_gx_su_ts0[coord & 7u];
    h = rotl1(h) ^ gc_gx_su_ts1[coord & 7u];
    h = rotl1(h) ^ gc_gx_bp_sent_not;
    h = rotl1(h) ^ gc_gx_last_ras_reg;
    return h;
}

static inline void dump_probe(uint8_t *out, u32 *off, u32 coord) {
    coord &= 7u;
    wr32be(out + (*off) * 4u, gc_gx_tcs_man_enab); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_su_ts0[coord]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_su_ts1[coord]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_bp_sent_not); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_last_ras_reg); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "GXSetTexCoordScaleManually/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tex_coord_scale_manually_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0xA0u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    seed_state();
    for (u32 i = 0; i < L0; i++) {
        GXSetTexCoordScaleManually(i, (u8)(i & 1u), (u16)(128u + i), (u16)(256u + i * 2u));
        a = rotl1(a) ^ hash_state(i);
    }
    tc += L0; m = rotl1(m) ^ a;

    seed_state();
    for (u32 i = 0; i < L1; i++) {
        u32 cidx = i & 7u;
        u8 en = (u8)(((i + 1u) >> 1) & 1u);
        GXSetTexCoordScaleManually(cidx, en, (u16)(0x80u + i * 3u), (u16)(0x100u + i * 5u));
        b = rotl1(b) ^ hash_state(cidx);
    }
    tc += L1; m = rotl1(m) ^ b;

    seed_state();
    for (u32 i = 0; i < L2; i++) {
        u32 cidx = i & 7u;
        GXSetTexCoordScaleManually(cidx, 1, (u16)(0x40u + i), (u16)(0x80u + i));
        GXSetTexCoordScaleManually(cidx, 1, (u16)(0xA0u + i), (u16)(0xC0u + i));
        c = rotl1(c) ^ hash_state(cidx);
    }
    tc += L2; m = rotl1(m) ^ c;

    seed_state();
    rs = 0xAC1D1234u;
    for (u32 i = 0; i < L3; i++) {
        u32 cidx = rn() & 15u;
        GXSetTexCoordScaleManually(cidx, (u8)(rn() & 1u), (u16)rn(), (u16)rn());
        d = rotl1(d) ^ hash_state(cidx);
    }
    tc += L3; m = rotl1(m) ^ d;

    seed_state();
    GXSetTexCoordScaleManually(0, 1, 128, 1025); e = rotl1(e) ^ hash_state(0);
    GXSetTexCoordScaleManually(1, 1, 1024, 1025); e = rotl1(e) ^ hash_state(1);
    GXSetTexCoordScaleManually(0, 0, 0, 0); e = rotl1(e) ^ hash_state(0);
    GXSetTexCoordScaleManually(1, 0, 0, 0); e = rotl1(e) ^ hash_state(1);
    GXSetTexCoordScaleManually(0, 1, 0x100, 0x101); e = rotl1(e) ^ hash_state(0);
    GXSetTexCoordScaleManually(1, 1, 0x100, 0x101); e = rotl1(e) ^ hash_state(1);
    GXSetTexCoordScaleManually(0, 1, 0x100, 0x100); e = rotl1(e) ^ hash_state(0);
    GXSetTexCoordScaleManually(0, 1, 256, 256); e = rotl1(e) ^ hash_state(0);
    tc += L4; m = rotl1(m) ^ e;

    seed_state();
    GXSetTexCoordScaleManually(7, 1, 1, 1); f = rotl1(f) ^ hash_state(7);
    GXSetTexCoordScaleManually(7, 1, 0, 0); f = rotl1(f) ^ hash_state(7);
    GXSetTexCoordScaleManually(8, 1, 512, 512); f = rotl1(f) ^ hash_state(7);
    GXSetTexCoordScaleManually(31, 0, 0, 0); f = rotl1(f) ^ hash_state(7);
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x47544353u); off++;
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    seed_state();
    GXSetTexCoordScaleManually(2, 1, 128, 256); dump_probe(out, &off, 2);
    GXSetTexCoordScaleManually(5, 1, 512, 64); dump_probe(out, &off, 5);
    GXSetTexCoordScaleManually(2, 0, 0, 0); dump_probe(out, &off, 2);
    GXSetTexCoordScaleManually(5, 0, 0, 0); dump_probe(out, &off, 5);
}
