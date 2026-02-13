#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;

void oracle_AISetStreamPlayState(u32 state);
u32 oracle_AIGetStreamPlayState(void);
u32 oracle_AIGetStreamSampleRate(void);
uint8_t oracle_AIGetStreamVolLeft(void);
uint8_t oracle_AIGetStreamVolRight(void);
void oracle_AISetStreamVolLeft(uint8_t volume);
void oracle_AISetStreamVolRight(uint8_t volume);

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

static inline void set_play(u32 s) { gc_ai_regs[0] = (gc_ai_regs[0] & ~1u) | (s & 1u); }
static inline void set_rate(u32 r) { gc_ai_regs[0] = (gc_ai_regs[0] & ~(1u << 1)) | ((r & 1u) << 1); }
static inline void set_rst(void) { gc_ai_regs[0] |= (1u << 5); }

static inline void reset_state(u32 s) {
    gc_ai_regs[0] = 0xA0000000u ^ s;
    gc_ai_regs[1] = 0xB0000000u ^ (s * 3u);
    gc_ai_regs[2] = 0xC0000000u ^ (s * 5u);
    gc_ai_regs[3] = 0xD0000000u ^ (s * 7u);
}

static inline u32 h(void) {
    u32 x = 0;
    x = rotl1(x) ^ gc_ai_regs[0];
    x = rotl1(x) ^ gc_ai_regs[1];
    return x;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off) {
    be(out + (*off) * 4u, gc_ai_regs[0]); (*off)++;
    be(out + (*off) * 4u, gc_ai_regs[1]); (*off)++;
    be(out + (*off) * 4u, oracle_AIGetStreamPlayState()); (*off)++;
    be(out + (*off) * 4u, oracle_AIGetStreamSampleRate()); (*off)++;
    be(out + (*off) * 4u, pack_lr(oracle_AIGetStreamVolLeft(), oracle_AIGetStreamVolRight())); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    reset_state(0u);
    set_rate(0u); set_play(0u);
    oracle_AISetStreamVolLeft(10); oracle_AISetStreamVolRight(200);
    oracle_AISetStreamPlayState(0u); a = rotl1(a) ^ h();

    reset_state(1u);
    set_rate(0u); set_play(0u);
    oracle_AISetStreamVolLeft(1); oracle_AISetStreamVolRight(2);
    oracle_AISetStreamPlayState(1u); a = rotl1(a) ^ h();

    reset_state(2u);
    set_rate(1u); set_play(0u);
    oracle_AISetStreamVolLeft(3); oracle_AISetStreamVolRight(4);
    oracle_AISetStreamPlayState(1u); a = rotl1(a) ^ h();

    reset_state(3u);
    set_rate(0u); set_play(1u);
    oracle_AISetStreamVolLeft(5); oracle_AISetStreamVolRight(6);
    oracle_AISetStreamPlayState(1u); a = rotl1(a) ^ h();

    reset_state(4u);
    set_rate(0u); set_play(1u);
    oracle_AISetStreamVolLeft(7); oracle_AISetStreamVolRight(8);
    oracle_AISetStreamPlayState(0u); a = rotl1(a) ^ h();

    reset_state(5u);
    set_rate(1u); set_play(1u);
    set_rst();
    oracle_AISetStreamVolLeft(9); oracle_AISetStreamVolRight(250);
    oracle_AISetStreamPlayState(0u); a = rotl1(a) ^ h();

    reset_state(6u);
    set_rate(0u); set_play(0u);
    oracle_AISetStreamVolLeft(0); oracle_AISetStreamVolRight(0);
    oracle_AISetStreamPlayState(1u); a = rotl1(a) ^ h();

    reset_state(7u);
    set_rate(0u); set_play(0u);
    set_rst();
    oracle_AISetStreamVolLeft(127); oracle_AISetStreamVolRight(128);
    oracle_AISetStreamPlayState(1u); a = rotl1(a) ^ h();

    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        set_rate(i & 1u);
        set_play((i >> 1) & 1u);
        oracle_AISetStreamVolLeft((uint8_t)(i * 7u));
        oracle_AISetStreamVolRight((uint8_t)(255u - i * 11u));
        oracle_AISetStreamPlayState((i & 1u) ? 1u : 0u);
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        set_rate(0u);
        set_play(0u);
        oracle_AISetStreamVolLeft((uint8_t)(i + 1u));
        oracle_AISetStreamVolRight((uint8_t)(0xF0u ^ i));
        oracle_AISetStreamPlayState(1u);
        oracle_AISetStreamPlayState(1u);
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA15E57A7u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        set_rate((rn() >> 3) & 1u);
        set_play((rn() >> 7) & 1u);
        oracle_AISetStreamVolLeft((uint8_t)rn());
        oracle_AISetStreamVolRight((uint8_t)rn());
        oracle_AISetStreamPlayState(rn() & 1u);
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    set_rate(0u); set_play(0u);
    oracle_AISetStreamVolLeft(10); oracle_AISetStreamVolRight(20); oracle_AISetStreamPlayState(1u); e = rotl1(e) ^ h();
    oracle_AISetStreamVolLeft(30); oracle_AISetStreamVolRight(40); oracle_AISetStreamPlayState(0u); e = rotl1(e) ^ h();
    set_rate(1u);
    oracle_AISetStreamVolLeft(50); oracle_AISetStreamVolRight(60); oracle_AISetStreamPlayState(1u); e = rotl1(e) ^ h();
    oracle_AISetStreamVolLeft(70); oracle_AISetStreamVolRight(80); oracle_AISetStreamPlayState(0u); e = rotl1(e) ^ h();
    set_rate(0u);
    oracle_AISetStreamVolLeft(90); oracle_AISetStreamVolRight(100); oracle_AISetStreamPlayState(1u); e = rotl1(e) ^ h();
    oracle_AISetStreamVolLeft(110); oracle_AISetStreamVolRight(120); oracle_AISetStreamPlayState(0u); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    set_rate(0u); set_play(0u);
    oracle_AISetStreamVolLeft(0); oracle_AISetStreamVolRight(255);
    oracle_AISetStreamPlayState(1u); f = rotl1(f) ^ h();
    oracle_AISetStreamPlayState(1u); f = rotl1(f) ^ h();
    oracle_AISetStreamPlayState(0u); f = rotl1(f) ^ h();
    oracle_AISetStreamPlayState(0u); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x41495350u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    reset_state(0x600u);
    set_rate(0u); set_play(0u);
    oracle_AISetStreamVolLeft(1); oracle_AISetStreamVolRight(2);
    oracle_AISetStreamPlayState(1u);
    dump_probe(out, &off);

    reset_state(0x601u);
    set_rate(1u); set_play(0u);
    oracle_AISetStreamVolLeft(1); oracle_AISetStreamVolRight(2);
    oracle_AISetStreamPlayState(1u);
    dump_probe(out, &off);

    reset_state(0x602u);
    set_rate(0u); set_play(1u);
    oracle_AISetStreamVolLeft(3); oracle_AISetStreamVolRight(4);
    oracle_AISetStreamPlayState(1u);
    dump_probe(out, &off);

    reset_state(0x603u);
    set_rate(0u); set_play(1u);
    oracle_AISetStreamVolLeft(5); oracle_AISetStreamVolRight(6);
    oracle_AISetStreamPlayState(0u);
    dump_probe(out, &off);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}

