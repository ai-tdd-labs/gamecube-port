#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef uint16_t u16;

u32 oracle_AIGetDMAStartAddr(void);
extern u16 gc_ai_dsp_regs[4];

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline void reset_state(u32 s) {
    gc_ai_dsp_regs[0] = (u16)(0xA000u ^ (u16)s);
    gc_ai_dsp_regs[1] = (u16)(0xB000u ^ (u16)(s * 3u));
    gc_ai_dsp_regs[2] = (u16)(0xC000u ^ (u16)(s * 5u));
    gc_ai_dsp_regs[3] = (u16)(0xD000u ^ (u16)(s * 7u));
}

static inline u32 h(u32 ret) {
    u32 x = 0;
    x = rotl1(x) ^ ret;
    x = rotl1(x) ^ (u32)gc_ai_dsp_regs[0];
    x = rotl1(x) ^ (u32)gc_ai_dsp_regs[1];
    return x;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off, u32 ret) {
    be(out + (*off) * 4u, (u32)gc_ai_dsp_regs[0]); (*off)++;
    be(out + (*off) * 4u, (u32)gc_ai_dsp_regs[1]); (*off)++;
    be(out + (*off) * 4u, ret); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    reset_state(0u);
    gc_ai_dsp_regs[0] = 0x0000u; gc_ai_dsp_regs[1] = 0x0000u; a = rotl1(a) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0xFFFFu; gc_ai_dsp_regs[1] = 0xFFFFu; a = rotl1(a) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x03FFu; gc_ai_dsp_regs[1] = 0x001Fu; a = rotl1(a) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0xFC00u; gc_ai_dsp_regs[1] = 0xFFE0u; a = rotl1(a) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x0400u; gc_ai_dsp_regs[1] = 0x0020u; a = rotl1(a) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x1234u; gc_ai_dsp_regs[1] = 0x5678u; a = rotl1(a) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x03FFu; gc_ai_dsp_regs[1] = 0xFFE0u; a = rotl1(a) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x0001u; gc_ai_dsp_regs[1] = 0x0001u; a = rotl1(a) ^ h(oracle_AIGetDMAStartAddr());
    tc += L0; m = rotl1(m) ^ a;

    rs = 0xBADC0FFEu;
    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        gc_ai_dsp_regs[0] = (u16)(rn() | 0xFC00u);
        gc_ai_dsp_regs[1] = (u16)(rn() | 0x001Fu);
        b = rotl1(b) ^ h(oracle_AIGetDMAStartAddr());
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        gc_ai_dsp_regs[0] = (u16)(i * 17u);
        gc_ai_dsp_regs[1] = (u16)(i * 33u);
        u32 r0 = oracle_AIGetDMAStartAddr();
        u32 r1 = oracle_AIGetDMAStartAddr();
        c = rotl1(c) ^ h(r0 ^ r1);
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA1D00D00u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        gc_ai_dsp_regs[0] = (u16)rn();
        gc_ai_dsp_regs[1] = (u16)rn();
        d = rotl1(d) ^ h(oracle_AIGetDMAStartAddr());
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    gc_ai_dsp_regs[0] = 0x0000u; gc_ai_dsp_regs[1] = 0x0020u; e = rotl1(e) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x03FFu; gc_ai_dsp_regs[1] = 0xFFE0u; e = rotl1(e) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x03FFu; gc_ai_dsp_regs[1] = 0x001Fu; e = rotl1(e) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x0000u; gc_ai_dsp_regs[1] = 0xFFFFu; e = rotl1(e) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0xFFFFu; gc_ai_dsp_regs[1] = 0x0000u; e = rotl1(e) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x8000u; gc_ai_dsp_regs[1] = 0x7FFFu; e = rotl1(e) ^ h(oracle_AIGetDMAStartAddr());
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    gc_ai_dsp_regs[0] = 0u; gc_ai_dsp_regs[1] = 0u; f = rotl1(f) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0x03FFu; gc_ai_dsp_regs[1] = 0x0000u; f = rotl1(f) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0u; gc_ai_dsp_regs[1] = 0xFFE0u; f = rotl1(f) ^ h(oracle_AIGetDMAStartAddr());
    gc_ai_dsp_regs[0] = 0xFFFFu; gc_ai_dsp_regs[1] = 0x001Fu; f = rotl1(f) ^ h(oracle_AIGetDMAStartAddr());
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x41494753u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    reset_state(0x600u); gc_ai_dsp_regs[0] = 0x03FFu; gc_ai_dsp_regs[1] = 0xFFFFu; dump_probe(out, &off, oracle_AIGetDMAStartAddr());
    reset_state(0x601u); gc_ai_dsp_regs[0] = 0xFC00u; gc_ai_dsp_regs[1] = 0x001Fu; dump_probe(out, &off, oracle_AIGetDMAStartAddr());
    reset_state(0x602u); gc_ai_dsp_regs[0] = 0x1234u; gc_ai_dsp_regs[1] = 0x5678u; dump_probe(out, &off, oracle_AIGetDMAStartAddr());
    reset_state(0x603u); gc_ai_dsp_regs[0] = 0xFFFFu; gc_ai_dsp_regs[1] = 0x0000u; dump_probe(out, &off, oracle_AIGetDMAStartAddr());

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
