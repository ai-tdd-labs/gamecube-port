#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

void oracle_GXSetTexCoordScaleManually_reset(void);
void oracle_GXSetTexCoordScaleManually(u32 coord, u8 enable, u16 ss, u16 ts);
extern u32 gc_gx_su_ts0[8];
extern u32 gc_gx_su_ts1[8];
extern u32 gc_gx_tcs_man_enab;
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

static inline u32 hash_state(u32 coord) {
    u32 h = 0;
    h = rotl1(h) ^ gc_gx_tcs_man_enab;
    h = rotl1(h) ^ gc_gx_su_ts0[coord & 7u];
    h = rotl1(h) ^ gc_gx_su_ts1[coord & 7u];
    h = rotl1(h) ^ gc_gx_bp_sent_not;
    h = rotl1(h) ^ gc_gx_last_ras_reg;
    return h;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off, u32 coord) {
    coord &= 7u;
    be(out + (*off) * 4u, gc_gx_tcs_man_enab); (*off)++;
    be(out + (*off) * 4u, gc_gx_su_ts0[coord]); (*off)++;
    be(out + (*off) * 4u, gc_gx_su_ts1[coord]); (*off)++;
    be(out + (*off) * 4u, gc_gx_bp_sent_not); (*off)++;
    be(out + (*off) * 4u, gc_gx_last_ras_reg); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    oracle_GXSetTexCoordScaleManually_reset();
    for (u32 i = 0; i < L0; i++) {
        oracle_GXSetTexCoordScaleManually(i, (u8)(i & 1u), (u16)(128u + i), (u16)(256u + i * 2u));
        a = rotl1(a) ^ hash_state(i);
    }
    tc += L0; m = rotl1(m) ^ a;

    oracle_GXSetTexCoordScaleManually_reset();
    for (u32 i = 0; i < L1; i++) {
        u32 cidx = i & 7u;
        u8 en = (u8)(((i + 1u) >> 1) & 1u);
        oracle_GXSetTexCoordScaleManually(cidx, en, (u16)(0x80u + i * 3u), (u16)(0x100u + i * 5u));
        b = rotl1(b) ^ hash_state(cidx);
    }
    tc += L1; m = rotl1(m) ^ b;

    oracle_GXSetTexCoordScaleManually_reset();
    for (u32 i = 0; i < L2; i++) {
        u32 cidx = i & 7u;
        oracle_GXSetTexCoordScaleManually(cidx, 1, (u16)(0x40u + i), (u16)(0x80u + i));
        oracle_GXSetTexCoordScaleManually(cidx, 1, (u16)(0xA0u + i), (u16)(0xC0u + i));
        c = rotl1(c) ^ hash_state(cidx);
    }
    tc += L2; m = rotl1(m) ^ c;

    oracle_GXSetTexCoordScaleManually_reset();
    rs = 0xAC1D1234u;
    for (u32 i = 0; i < L3; i++) {
        u32 cidx = rn() & 15u;
        oracle_GXSetTexCoordScaleManually(cidx, (u8)(rn() & 1u), (u16)rn(), (u16)rn());
        d = rotl1(d) ^ hash_state(cidx);
    }
    tc += L3; m = rotl1(m) ^ d;

    oracle_GXSetTexCoordScaleManually_reset();
    oracle_GXSetTexCoordScaleManually(0, 1, 128, 1025); e = rotl1(e) ^ hash_state(0);
    oracle_GXSetTexCoordScaleManually(1, 1, 1024, 1025); e = rotl1(e) ^ hash_state(1);
    oracle_GXSetTexCoordScaleManually(0, 0, 0, 0); e = rotl1(e) ^ hash_state(0);
    oracle_GXSetTexCoordScaleManually(1, 0, 0, 0); e = rotl1(e) ^ hash_state(1);
    oracle_GXSetTexCoordScaleManually(0, 1, 0x100, 0x101); e = rotl1(e) ^ hash_state(0);
    oracle_GXSetTexCoordScaleManually(1, 1, 0x100, 0x101); e = rotl1(e) ^ hash_state(1);
    oracle_GXSetTexCoordScaleManually(0, 1, 0x100, 0x100); e = rotl1(e) ^ hash_state(0);
    oracle_GXSetTexCoordScaleManually(0, 1, 256, 256); e = rotl1(e) ^ hash_state(0);
    tc += L4; m = rotl1(m) ^ e;

    oracle_GXSetTexCoordScaleManually_reset();
    oracle_GXSetTexCoordScaleManually(7, 1, 1, 1); f = rotl1(f) ^ hash_state(7);
    oracle_GXSetTexCoordScaleManually(7, 1, 0, 0); f = rotl1(f) ^ hash_state(7);
    oracle_GXSetTexCoordScaleManually(8, 1, 512, 512); f = rotl1(f) ^ hash_state(7);
    oracle_GXSetTexCoordScaleManually(31, 0, 0, 0); f = rotl1(f) ^ hash_state(7);
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x47544353u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    oracle_GXSetTexCoordScaleManually_reset();
    oracle_GXSetTexCoordScaleManually(2, 1, 128, 256); dump_probe(out, &off, 2);
    oracle_GXSetTexCoordScaleManually(5, 1, 512, 64); dump_probe(out, &off, 5);
    oracle_GXSetTexCoordScaleManually(2, 0, 0, 0); dump_probe(out, &off, 2);
    oracle_GXSetTexCoordScaleManually(5, 0, 0, 0); dump_probe(out, &off, 5);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
