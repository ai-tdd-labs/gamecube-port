#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;

void oracle_ARStartDMA_reset(u32 s);
void oracle_ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length);
extern u32 gc_ar_dma_type;
extern u32 gc_ar_dma_mainmem;
extern u32 gc_ar_dma_aram;
extern u32 gc_ar_dma_length;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline u32 h(void) {
    u32 x = 0;
    x = rotl1(x) ^ gc_ar_dma_type;
    x = rotl1(x) ^ gc_ar_dma_mainmem;
    x = rotl1(x) ^ gc_ar_dma_aram;
    x = rotl1(x) ^ gc_ar_dma_length;
    return x;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off) {
    be(out + (*off) * 4u, gc_ar_dma_type); (*off)++;
    be(out + (*off) * 4u, gc_ar_dma_mainmem); (*off)++;
    be(out + (*off) * 4u, gc_ar_dma_aram); (*off)++;
    be(out + (*off) * 4u, gc_ar_dma_length); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    for (u32 i = 0; i < L0; i++) {
        oracle_ARStartDMA_reset(i);
        oracle_ARStartDMA(i & 1u, 0x80000000u + i * 0x100u, 0x00004000u + i * 0x20u, 0x20u << (i & 3u));
        a = rotl1(a) ^ h();
    }
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        oracle_ARStartDMA_reset(0x100u + i);
        oracle_ARStartDMA(i & 1u, 0x80100000u + i * 0x80u, 0x00008000u + i * 0x40u, 0x100u + i * 0x10u);
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        oracle_ARStartDMA_reset(0x200u + i);
        u32 t = i & 1u;
        u32 mm = 0x80200000u + i * 0x200u;
        u32 ar = 0x0000A000u + i * 0x80u;
        u32 ln = 0x200u + i * 0x20u;
        oracle_ARStartDMA(t, mm, ar, ln);
        oracle_ARStartDMA(t, mm, ar, ln);
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xC001D00Du;
    for (u32 i = 0; i < L3; i++) {
        oracle_ARStartDMA_reset(rn());
        oracle_ARStartDMA(rn() & 1u, rn(), rn(), rn());
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    oracle_ARStartDMA_reset(0x400u);
    oracle_ARStartDMA(0u, 0x80010000u, 0x00004000u, 0x00001000u); e = rotl1(e) ^ h();
    oracle_ARStartDMA(1u, 0x80020000u, 0x00008000u, 0x00002000u); e = rotl1(e) ^ h();
    oracle_ARStartDMA(0u, 0x80030000u, 0x0000C000u, 0x00000100u); e = rotl1(e) ^ h();
    oracle_ARStartDMA(1u, 0x80040000u, 0x00010000u, 0x00000200u); e = rotl1(e) ^ h();
    oracle_ARStartDMA(0u, 0x80050000u, 0x00014000u, 0x00000400u); e = rotl1(e) ^ h();
    oracle_ARStartDMA(1u, 0x80060000u, 0x00018000u, 0x00000800u); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    oracle_ARStartDMA_reset(0x500u);
    oracle_ARStartDMA(0xFFFFFFFFu, 0x00000000u, 0xFFFFFFFFu, 0x00000000u); f = rotl1(f) ^ h();
    oracle_ARStartDMA(0x00000002u, 0xFFFFFFFFu, 0x00000000u, 0xFFFFFFFFu); f = rotl1(f) ^ h();
    oracle_ARStartDMA(0u, 0u, 0u, 0u); f = rotl1(f) ^ h();
    oracle_ARStartDMA(1u, 1u, 1u, 1u); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x4152444Du); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    oracle_ARStartDMA_reset(0x600u);
    oracle_ARStartDMA(0u, 0x80100000u, 0x00004000u, 0x00001000u); dump_probe(out, &off);
    oracle_ARStartDMA(1u, 0x80200000u, 0x00008000u, 0x00002000u); dump_probe(out, &off);
    oracle_ARStartDMA(0u, 0x80300000u, 0x0000C000u, 0x00004000u); dump_probe(out, &off);
    oracle_ARStartDMA(1u, 0x80400000u, 0x00010000u, 0x00008000u); dump_probe(out, &off);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
