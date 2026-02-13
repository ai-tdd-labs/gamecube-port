#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;

void oracle_ARGetDMAStatus_reset(u32 s);
u32 oracle_ARGetDMAStatus(void);
extern u32 gc_ar_dma_status;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 8, L5 = 4 };

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline u32 h(u32 ret) {
    u32 x = 0;
    x = rotl1(x) ^ gc_ar_dma_status;
    x = rotl1(x) ^ ret;
    return x;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off, u32 ret) {
    be(out + (*off) * 4u, gc_ar_dma_status); (*off)++;
    be(out + (*off) * 4u, ret); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    for (u32 i = 0; i < L0; i++) {
        oracle_ARGetDMAStatus_reset(i);
        u32 r = oracle_ARGetDMAStatus();
        a = rotl1(a) ^ h(r);
    }
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        oracle_ARGetDMAStatus_reset(0x100u + i);
        u32 r0 = oracle_ARGetDMAStatus();
        u32 r1 = oracle_ARGetDMAStatus();
        b = rotl1(b) ^ h(r0 ^ r1);
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        oracle_ARGetDMAStatus_reset(0x200u + i);
        u32 r0 = oracle_ARGetDMAStatus();
        oracle_ARGetDMAStatus_reset(0x200u + i);
        u32 r1 = oracle_ARGetDMAStatus();
        c = rotl1(c) ^ h(r0 ^ r1);
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xD15EA5Eu;
    for (u32 i = 0; i < L3; i++) {
        oracle_ARGetDMAStatus_reset(rn());
        u32 r = oracle_ARGetDMAStatus();
        d = rotl1(d) ^ h(r);
    }
    tc += L3; m = rotl1(m) ^ d;

    oracle_ARGetDMAStatus_reset(0u); e = rotl1(e) ^ h(oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(1u); e = rotl1(e) ^ h(oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(0xFFFFFFFFu); e = rotl1(e) ^ h(oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(0x80000000u); e = rotl1(e) ^ h(oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(0x7FFFFFFFu); e = rotl1(e) ^ h(oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(0x00000200u); e = rotl1(e) ^ h(oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(0x00000000u); e = rotl1(e) ^ h(oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(0x00000200u); e = rotl1(e) ^ h(oracle_ARGetDMAStatus());
    tc += L4; m = rotl1(m) ^ e;

    oracle_ARGetDMAStatus_reset(0x12345678u);
    u32 r = oracle_ARGetDMAStatus();
    f = rotl1(f) ^ h(r);
    r = oracle_ARGetDMAStatus();
    f = rotl1(f) ^ h(r);
    oracle_ARGetDMAStatus_reset(0x12345678u);
    r = oracle_ARGetDMAStatus();
    f = rotl1(f) ^ h(r);
    oracle_ARGetDMAStatus_reset(0x00000000u);
    r = oracle_ARGetDMAStatus();
    f = rotl1(f) ^ h(r);
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x41524453u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    oracle_ARGetDMAStatus_reset(0x00000200u);
    dump_probe(out, &off, oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(0x00000000u);
    dump_probe(out, &off, oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(0xFFFFFFFFu);
    dump_probe(out, &off, oracle_ARGetDMAStatus());
    oracle_ARGetDMAStatus_reset(0x7FFFFFFFu);
    dump_probe(out, &off, oracle_ARGetDMAStatus());

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
