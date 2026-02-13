#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha);
extern u32 gc_gx_tev_ksel[8];
extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_last_ras_reg;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 8, L5 = 4 };

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline void seed(u32 s) {
    for (u32 i = 0; i < 8u; i++) {
        gc_gx_tev_ksel[i] = (s << (i & 7u)) ^ (0x01010101u * (i + 1u));
    }
    gc_gx_bp_sent_not = s ^ 0xA5A5A5A5u;
    gc_gx_last_ras_reg = s ^ 0x5A5A5A5Au;
}

static inline u32 h(void) {
    u32 x = 0;
    x = rotl1(x) ^ gc_gx_tev_ksel[0];
    x = rotl1(x) ^ gc_gx_tev_ksel[1];
    x = rotl1(x) ^ gc_gx_tev_ksel[4];
    x = rotl1(x) ^ gc_gx_tev_ksel[5];
    x = rotl1(x) ^ gc_gx_bp_sent_not;
    x = rotl1(x) ^ gc_gx_last_ras_reg;
    return x;
}

static inline void dump_probe(uint8_t *out, u32 *off) {
    wr32be(out + (*off) * 4u, gc_gx_tev_ksel[0]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_tev_ksel[1]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_tev_ksel[4]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_tev_ksel[5]); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_bp_sent_not); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_last_ras_reg); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "GXSetTevSwapModeTable/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tev_swap_mode_table_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x9Cu);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    seed(0u);
    GXSetTevSwapModeTable(0u, 0u, 1u, 2u, 3u); a = rotl1(a) ^ h();
    GXSetTevSwapModeTable(1u, 3u, 2u, 1u, 0u); a = rotl1(a) ^ h();
    GXSetTevSwapModeTable(2u, 1u, 1u, 1u, 1u); a = rotl1(a) ^ h();
    GXSetTevSwapModeTable(3u, 2u, 2u, 2u, 2u); a = rotl1(a) ^ h();
    GXSetTevSwapModeTable(4u, 3u, 3u, 3u, 3u); a = rotl1(a) ^ h();
    GXSetTevSwapModeTable(7u, 0u, 0u, 0u, 0u); a = rotl1(a) ^ h();
    GXSetTevSwapModeTable(0u, 4u, 5u, 6u, 7u); a = rotl1(a) ^ h();
    GXSetTevSwapModeTable(1u, 0xFFFFFFFFu, 0xFFFFFFFEu, 0xFFFFFFFDu, 0xFFFFFFFCu); a = rotl1(a) ^ h();
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        seed(0x100u + i);
        GXSetTevSwapModeTable(i & 3u, i, i + 1u, i + 2u, i + 3u);
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        seed(0x200u + i);
        u32 t = i & 3u;
        GXSetTevSwapModeTable(t, i, i + 1u, i + 2u, i + 3u);
        GXSetTevSwapModeTable(t, i, i + 1u, i + 2u, i + 3u);
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0x1234ABCDu;
    for (u32 i = 0; i < L3; i++) {
        seed(rn());
        GXSetTevSwapModeTable(rn() & 7u, rn(), rn(), rn(), rn());
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    seed(0x400u);
    GXSetTevSwapModeTable(0u, 0u, 1u, 2u, 3u); e = rotl1(e) ^ h();
    GXSetTevSwapModeTable(1u, 0u, 0u, 0u, 3u); e = rotl1(e) ^ h();
    GXSetTevSwapModeTable(2u, 1u, 1u, 1u, 3u); e = rotl1(e) ^ h();
    GXSetTevSwapModeTable(3u, 2u, 2u, 2u, 3u); e = rotl1(e) ^ h();
    GXSetTevSwapModeTable(3u, 3u, 3u, 3u, 3u); e = rotl1(e) ^ h();
    GXSetTevSwapModeTable(2u, 0u, 1u, 0u, 1u); e = rotl1(e) ^ h();
    GXSetTevSwapModeTable(1u, 2u, 3u, 2u, 3u); e = rotl1(e) ^ h();
    GXSetTevSwapModeTable(0u, 3u, 2u, 1u, 0u); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    seed(0x500u);
    GXSetTevSwapModeTable(0xFFFFFFFFu, 0, 0, 0, 0); f = rotl1(f) ^ h();
    GXSetTevSwapModeTable(4u, 1, 1, 1, 1); f = rotl1(f) ^ h();
    GXSetTevSwapModeTable(3u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu); f = rotl1(f) ^ h();
    GXSetTevSwapModeTable(2u, 0u, 0u, 0u, 0u); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x47535442u); off++;
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    seed(0x600u);
    GXSetTevSwapModeTable(0u, 0u, 1u, 2u, 3u); dump_probe(out, &off);
    GXSetTevSwapModeTable(1u, 3u, 2u, 1u, 0u); dump_probe(out, &off);
    GXSetTevSwapModeTable(2u, 1u, 1u, 1u, 3u); dump_probe(out, &off);
    GXSetTevSwapModeTable(3u, 2u, 2u, 2u, 3u); dump_probe(out, &off);
}
