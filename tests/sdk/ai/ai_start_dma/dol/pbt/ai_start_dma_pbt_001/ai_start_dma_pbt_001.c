#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef uint16_t u16;

void oracle_AIStartDMA(void);
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

static inline u32 h(void) {
    u32 x = 0;
    x = rotl1(x) ^ (u32)gc_ai_dsp_regs[3];
    return x;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off) {
    be(out + (*off) * 4u, (u32)gc_ai_dsp_regs[3]); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    reset_state(0u);
    gc_ai_dsp_regs[3] = 0x0000u; oracle_AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(1u);
    gc_ai_dsp_regs[3] = 0x7FFFu; oracle_AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(2u);
    gc_ai_dsp_regs[3] = 0x8000u; oracle_AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(3u);
    gc_ai_dsp_regs[3] = 0xFFFFu; oracle_AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(4u);
    gc_ai_dsp_regs[3] = 0x1234u; oracle_AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(5u);
    gc_ai_dsp_regs[3] = 0x8001u; oracle_AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(6u);
    gc_ai_dsp_regs[3] = 0x0001u; oracle_AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(7u);
    gc_ai_dsp_regs[3] = 0x4000u; oracle_AIStartDMA(); a = rotl1(a) ^ h();
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        gc_ai_dsp_regs[3] = (u16)(i * 0x1111u);
        oracle_AIStartDMA();
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        gc_ai_dsp_regs[3] = (u16)(0x1234u ^ (u16)i);
        oracle_AIStartDMA();
        oracle_AIStartDMA();
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA15D7A27u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        gc_ai_dsp_regs[3] = (u16)rn();
        oracle_AIStartDMA();
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    gc_ai_dsp_regs[3] = 0u; oracle_AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 1u; oracle_AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 0x7FFFu; oracle_AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 0x8000u; oracle_AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 0x8001u; oracle_AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 0xFFFFu; oracle_AIStartDMA(); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    gc_ai_dsp_regs[3] = 0u; oracle_AIStartDMA(); f = rotl1(f) ^ h();
    oracle_AIStartDMA(); f = rotl1(f) ^ h();
    gc_ai_dsp_regs[3] = 0x7FFFu; oracle_AIStartDMA(); f = rotl1(f) ^ h();
    oracle_AIStartDMA(); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x41495344u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    reset_state(0x600u); gc_ai_dsp_regs[3] = 0x0000u; oracle_AIStartDMA(); dump_probe(out, &off);
    reset_state(0x601u); gc_ai_dsp_regs[3] = 0x7FFFu; oracle_AIStartDMA(); dump_probe(out, &off);
    reset_state(0x602u); gc_ai_dsp_regs[3] = 0x8000u; oracle_AIStartDMA(); dump_probe(out, &off);
    reset_state(0x603u); gc_ai_dsp_regs[3] = 0xFFFFu; oracle_AIStartDMA(); dump_probe(out, &off);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}

