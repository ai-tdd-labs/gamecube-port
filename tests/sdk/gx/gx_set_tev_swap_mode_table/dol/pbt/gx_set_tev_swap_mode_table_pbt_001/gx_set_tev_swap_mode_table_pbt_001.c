#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;

void oracle_GXSetTevSwapModeTable_reset(u32 s);
void oracle_GXSetTevSwapModeTable(u32 table, u32 red, u32 green, u32 blue, u32 alpha);
extern u32 gc_gx_tev_ksel[8];
extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_last_ras_reg;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 8, L5 = 4 };

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

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

static inline void dump_probe(volatile uint8_t *out, u32 *off) {
    be(out + (*off) * 4u, gc_gx_tev_ksel[0]); (*off)++;
    be(out + (*off) * 4u, gc_gx_tev_ksel[1]); (*off)++;
    be(out + (*off) * 4u, gc_gx_tev_ksel[4]); (*off)++;
    be(out + (*off) * 4u, gc_gx_tev_ksel[5]); (*off)++;
    be(out + (*off) * 4u, gc_gx_bp_sent_not); (*off)++;
    be(out + (*off) * 4u, gc_gx_last_ras_reg); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    oracle_GXSetTevSwapModeTable_reset(0u);
    oracle_GXSetTevSwapModeTable(0u, 0u, 1u, 2u, 3u); a = rotl1(a) ^ h();
    oracle_GXSetTevSwapModeTable(1u, 3u, 2u, 1u, 0u); a = rotl1(a) ^ h();
    oracle_GXSetTevSwapModeTable(2u, 1u, 1u, 1u, 1u); a = rotl1(a) ^ h();
    oracle_GXSetTevSwapModeTable(3u, 2u, 2u, 2u, 2u); a = rotl1(a) ^ h();
    oracle_GXSetTevSwapModeTable(4u, 3u, 3u, 3u, 3u); a = rotl1(a) ^ h();
    oracle_GXSetTevSwapModeTable(7u, 0u, 0u, 0u, 0u); a = rotl1(a) ^ h();
    oracle_GXSetTevSwapModeTable(0u, 4u, 5u, 6u, 7u); a = rotl1(a) ^ h();
    oracle_GXSetTevSwapModeTable(1u, 0xFFFFFFFFu, 0xFFFFFFFEu, 0xFFFFFFFDu, 0xFFFFFFFCu); a = rotl1(a) ^ h();
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        oracle_GXSetTevSwapModeTable_reset(0x100u + i);
        oracle_GXSetTevSwapModeTable(i & 3u, i, i + 1u, i + 2u, i + 3u);
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        oracle_GXSetTevSwapModeTable_reset(0x200u + i);
        u32 t = i & 3u;
        oracle_GXSetTevSwapModeTable(t, i, i + 1u, i + 2u, i + 3u);
        oracle_GXSetTevSwapModeTable(t, i, i + 1u, i + 2u, i + 3u);
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0x1234ABCDu;
    for (u32 i = 0; i < L3; i++) {
        oracle_GXSetTevSwapModeTable_reset(rn());
        oracle_GXSetTevSwapModeTable(rn() & 7u, rn(), rn(), rn(), rn());
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    oracle_GXSetTevSwapModeTable_reset(0x400u);
    oracle_GXSetTevSwapModeTable(0u, 0u, 1u, 2u, 3u); e = rotl1(e) ^ h();
    oracle_GXSetTevSwapModeTable(1u, 0u, 0u, 0u, 3u); e = rotl1(e) ^ h();
    oracle_GXSetTevSwapModeTable(2u, 1u, 1u, 1u, 3u); e = rotl1(e) ^ h();
    oracle_GXSetTevSwapModeTable(3u, 2u, 2u, 2u, 3u); e = rotl1(e) ^ h();
    oracle_GXSetTevSwapModeTable(3u, 3u, 3u, 3u, 3u); e = rotl1(e) ^ h();
    oracle_GXSetTevSwapModeTable(2u, 0u, 1u, 0u, 1u); e = rotl1(e) ^ h();
    oracle_GXSetTevSwapModeTable(1u, 2u, 3u, 2u, 3u); e = rotl1(e) ^ h();
    oracle_GXSetTevSwapModeTable(0u, 3u, 2u, 1u, 0u); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    oracle_GXSetTevSwapModeTable_reset(0x500u);
    oracle_GXSetTevSwapModeTable(0xFFFFFFFFu, 0, 0, 0, 0); f = rotl1(f) ^ h();
    oracle_GXSetTevSwapModeTable(4u, 1, 1, 1, 1); f = rotl1(f) ^ h();
    oracle_GXSetTevSwapModeTable(3u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu); f = rotl1(f) ^ h();
    oracle_GXSetTevSwapModeTable(2u, 0u, 0u, 0u, 0u); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x47535442u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    oracle_GXSetTevSwapModeTable_reset(0x600u);
    oracle_GXSetTevSwapModeTable(0u, 0u, 1u, 2u, 3u); dump_probe(out, &off);
    oracle_GXSetTevSwapModeTable(1u, 3u, 2u, 1u, 0u); dump_probe(out, &off);
    oracle_GXSetTevSwapModeTable(2u, 1u, 1u, 1u, 3u); dump_probe(out, &off);
    oracle_GXSetTevSwapModeTable(3u, 2u, 2u, 2u, 3u); dump_probe(out, &off);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
