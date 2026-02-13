#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void AISetStreamVolLeft(uint8_t volume);
uint8_t AIGetStreamVolLeft(void);
uint8_t AIGetStreamVolRight(void);
extern u32 gc_ai_regs[4];

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

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
    x = rotl1(x) ^ pack_lr(AIGetStreamVolLeft(), AIGetStreamVolRight());
    return x;
}

static inline void dump_probe(uint8_t *out, u32 *off) {
    wr32be(out + (*off) * 4u, gc_ai_regs[1]); (*off)++;
    wr32be(out + (*off) * 4u, pack_lr(AIGetStreamVolLeft(), AIGetStreamVolRight())); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "AISetStreamVolLeft/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/ai_set_stream_vol_left_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x60u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    reset_state(0u);
    gc_ai_regs[1] = 0x0000FF00u;
    AISetStreamVolLeft(0); a = rotl1(a) ^ h();
    AISetStreamVolLeft(1); a = rotl1(a) ^ h();
    AISetStreamVolLeft(127); a = rotl1(a) ^ h();
    AISetStreamVolLeft(128); a = rotl1(a) ^ h();
    AISetStreamVolLeft(255); a = rotl1(a) ^ h();
    AISetStreamVolLeft(0); a = rotl1(a) ^ h();
    AISetStreamVolLeft(255); a = rotl1(a) ^ h();
    AISetStreamVolLeft((uint8_t)0xAA); a = rotl1(a) ^ h();
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        AISetStreamVolLeft((uint8_t)(i * 13u));
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        uint8_t v = (uint8_t)(0xF0u ^ i);
        AISetStreamVolLeft(v);
        AISetStreamVolLeft(v);
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA10C3F7u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        AISetStreamVolLeft((uint8_t)rn());
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    for (u32 i = 0; i < L4; i++) {
        AISetStreamVolLeft((uint8_t)(i * 37u));
        e = rotl1(e) ^ h();
    }
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    AISetStreamVolLeft(0); f = rotl1(f) ^ h();
    AISetStreamVolLeft(255); f = rotl1(f) ^ h();
    AISetStreamVolLeft(0); f = rotl1(f) ^ h();
    AISetStreamVolLeft(255); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x4149564Cu); off++; /* AIVL */
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    reset_state(0x600u); gc_ai_regs[1] = 0x12345678u; AISetStreamVolLeft(0); dump_probe(out, &off);
    reset_state(0x601u); gc_ai_regs[1] = 0x12345678u; AISetStreamVolLeft(255); dump_probe(out, &off);
    reset_state(0x602u); gc_ai_regs[1] = 0xFFFF0000u; AISetStreamVolLeft(1); dump_probe(out, &off);
    reset_state(0x603u); gc_ai_regs[1] = 0x0000FF00u; AISetStreamVolLeft(2); dump_probe(out, &off);
}
