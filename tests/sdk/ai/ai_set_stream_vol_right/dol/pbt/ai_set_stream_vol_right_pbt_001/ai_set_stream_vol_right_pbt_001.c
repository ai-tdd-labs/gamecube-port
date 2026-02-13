#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;

void oracle_AISetStreamVolRight(uint8_t volume);
uint8_t oracle_AIGetStreamVolLeft(void);
uint8_t oracle_AIGetStreamVolRight(void);
extern u32 gc_ai_regs[4];

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline u32 pack_lr(uint8_t l, uint8_t r) { return ((u32)r << 8) | (u32)l; }

static inline void reset_state(u32 s) {
    gc_ai_regs[0] = 0xA0000000u ^ s;
    gc_ai_regs[1] = 0xB0000000u ^ (s * 3u);
    gc_ai_regs[2] = 0xC0000000u ^ (s * 5u);
    gc_ai_regs[3] = 0xD0000000u ^ (s * 7u);
}

static inline u32 h(void) {
    u32 x = 0;
    x = rotl1(x) ^ gc_ai_regs[1];
    x = rotl1(x) ^ pack_lr(oracle_AIGetStreamVolLeft(), oracle_AIGetStreamVolRight());
    return x;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off) {
    be(out + (*off) * 4u, gc_ai_regs[1]); (*off)++;
    be(out + (*off) * 4u, pack_lr(oracle_AIGetStreamVolLeft(), oracle_AIGetStreamVolRight())); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    reset_state(0u);
    gc_ai_regs[1] = 0x000000FFu;
    oracle_AISetStreamVolRight(0); a = rotl1(a) ^ h();
    oracle_AISetStreamVolRight(1); a = rotl1(a) ^ h();
    oracle_AISetStreamVolRight(127); a = rotl1(a) ^ h();
    oracle_AISetStreamVolRight(128); a = rotl1(a) ^ h();
    oracle_AISetStreamVolRight(255); a = rotl1(a) ^ h();
    oracle_AISetStreamVolRight(0); a = rotl1(a) ^ h();
    oracle_AISetStreamVolRight(255); a = rotl1(a) ^ h();
    oracle_AISetStreamVolRight((uint8_t)0xAA); a = rotl1(a) ^ h();
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        oracle_AISetStreamVolRight((uint8_t)(i * 13u));
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        uint8_t v = (uint8_t)(0xF0u ^ i);
        oracle_AISetStreamVolRight(v);
        oracle_AISetStreamVolRight(v);
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA10C3F7u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        oracle_AISetStreamVolRight((uint8_t)rn());
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    for (u32 i = 0; i < L4; i++) {
        oracle_AISetStreamVolRight((uint8_t)(i * 37u));
        e = rotl1(e) ^ h();
    }
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    oracle_AISetStreamVolRight(0); f = rotl1(f) ^ h();
    oracle_AISetStreamVolRight(255); f = rotl1(f) ^ h();
    oracle_AISetStreamVolRight(0); f = rotl1(f) ^ h();
    oracle_AISetStreamVolRight(255); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x41495652u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    reset_state(0x600u); gc_ai_regs[1] = 0x12345678u; oracle_AISetStreamVolRight(0); dump_probe(out, &off);
    reset_state(0x601u); gc_ai_regs[1] = 0x12345678u; oracle_AISetStreamVolRight(255); dump_probe(out, &off);
    reset_state(0x602u); gc_ai_regs[1] = 0xFFFF0000u; oracle_AISetStreamVolRight(1); dump_probe(out, &off);
    reset_state(0x603u); gc_ai_regs[1] = 0x0000FF00u; oracle_AISetStreamVolRight(2); dump_probe(out, &off);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}

