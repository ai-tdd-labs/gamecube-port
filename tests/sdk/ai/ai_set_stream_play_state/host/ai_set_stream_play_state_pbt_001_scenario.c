#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

void AISetStreamPlayState(u32 state);
u32 AIGetStreamPlayState(void);
u32 AIGetStreamSampleRate(void);
uint8_t AIGetStreamVolLeft(void);
uint8_t AIGetStreamVolRight(void);
void AISetStreamVolLeft(uint8_t volume);
void AISetStreamVolRight(uint8_t volume);

extern u32 gc_ai_regs[4];

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

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

static inline void dump_probe(uint8_t *out, u32 *off) {
    wr32be(out + (*off) * 4u, gc_ai_regs[0]); (*off)++;
    wr32be(out + (*off) * 4u, gc_ai_regs[1]); (*off)++;
    wr32be(out + (*off) * 4u, AIGetStreamPlayState()); (*off)++;
    wr32be(out + (*off) * 4u, AIGetStreamSampleRate()); (*off)++;
    wr32be(out + (*off) * 4u, pack_lr(AIGetStreamVolLeft(), AIGetStreamVolRight())); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "AISetStreamPlayState/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/ai_set_stream_play_state_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x90u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    /* L0: direct edge matrix: no-op, special case (rate=0,start=1), and normal case. */
    reset_state(0u);
    set_rate(0u); set_play(0u);
    AISetStreamVolLeft(10); AISetStreamVolRight(200);
    AISetStreamPlayState(0u); a = rotl1(a) ^ h(); /* no-op */

    reset_state(1u);
    set_rate(0u); set_play(0u);
    AISetStreamVolLeft(1); AISetStreamVolRight(2);
    AISetStreamPlayState(1u); a = rotl1(a) ^ h(); /* special case swap + bit5 */

    reset_state(2u);
    set_rate(1u); set_play(0u);
    AISetStreamVolLeft(3); AISetStreamVolRight(4);
    AISetStreamPlayState(1u); a = rotl1(a) ^ h(); /* normal set */

    reset_state(3u);
    set_rate(0u); set_play(1u);
    AISetStreamVolLeft(5); AISetStreamVolRight(6);
    AISetStreamPlayState(1u); a = rotl1(a) ^ h(); /* no-op */

    reset_state(4u);
    set_rate(0u); set_play(1u);
    AISetStreamVolLeft(7); AISetStreamVolRight(8);
    AISetStreamPlayState(0u); a = rotl1(a) ^ h(); /* normal stop */

    reset_state(5u);
    set_rate(1u); set_play(1u);
    set_rst();
    AISetStreamVolLeft(9); AISetStreamVolRight(250);
    AISetStreamPlayState(0u); a = rotl1(a) ^ h();

    reset_state(6u);
    set_rate(0u); set_play(0u);
    AISetStreamVolLeft(0); AISetStreamVolRight(0);
    AISetStreamPlayState(1u); a = rotl1(a) ^ h();

    reset_state(7u);
    set_rate(0u); set_play(0u);
    set_rst();
    AISetStreamVolLeft(127); AISetStreamVolRight(128);
    AISetStreamPlayState(1u); a = rotl1(a) ^ h();

    tc += L0; m = rotl1(m) ^ a;

    /* L1: accumulation over deterministic sweep (rate toggles; start/stop). */
    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        set_rate(i & 1u);
        set_play((i >> 1) & 1u);
        AISetStreamVolLeft((uint8_t)(i * 7u));
        AISetStreamVolRight((uint8_t)(255u - i * 11u));
        AISetStreamPlayState((i & 1u) ? 1u : 0u);
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    /* L2: idempotency (apply same state twice). */
    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        set_rate(0u);
        set_play(0u);
        AISetStreamVolLeft((uint8_t)(i + 1u));
        AISetStreamVolRight((uint8_t)(0xF0u ^ i));
        AISetStreamPlayState(1u);
        AISetStreamPlayState(1u);
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    /* L3: seeded pseudo-random states (deterministic). */
    rs = 0xA15E57A7u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        set_rate((rn() >> 3) & 1u);
        set_play((rn() >> 7) & 1u);
        AISetStreamVolLeft((uint8_t)rn());
        AISetStreamVolRight((uint8_t)rn());
        AISetStreamPlayState(rn() & 1u);
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    /* L4: callsite-style sequences (start/stop around volume changes). */
    reset_state(0x400u);
    set_rate(0u); set_play(0u);
    AISetStreamVolLeft(10); AISetStreamVolRight(20); AISetStreamPlayState(1u); e = rotl1(e) ^ h();
    AISetStreamVolLeft(30); AISetStreamVolRight(40); AISetStreamPlayState(0u); e = rotl1(e) ^ h();
    set_rate(1u);
    AISetStreamVolLeft(50); AISetStreamVolRight(60); AISetStreamPlayState(1u); e = rotl1(e) ^ h();
    AISetStreamVolLeft(70); AISetStreamVolRight(80); AISetStreamPlayState(0u); e = rotl1(e) ^ h();
    set_rate(0u);
    AISetStreamVolLeft(90); AISetStreamVolRight(100); AISetStreamPlayState(1u); e = rotl1(e) ^ h();
    AISetStreamVolLeft(110); AISetStreamVolRight(120); AISetStreamPlayState(0u); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    /* L5: boundary/no-op invariants and max volumes. */
    reset_state(0x500u);
    set_rate(0u); set_play(0u);
    AISetStreamVolLeft(0); AISetStreamVolRight(255);
    AISetStreamPlayState(1u); f = rotl1(f) ^ h();
    AISetStreamPlayState(1u); f = rotl1(f) ^ h();
    AISetStreamPlayState(0u); f = rotl1(f) ^ h();
    AISetStreamPlayState(0u); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x41495350u); off++; /* AISP */
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    /* Probes: ensure swap behavior + bit effects are observable in final state. */
    reset_state(0x600u);
    set_rate(0u); set_play(0u);
    AISetStreamVolLeft(1); AISetStreamVolRight(2);
    AISetStreamPlayState(1u);
    dump_probe(out, &off);

    reset_state(0x601u);
    set_rate(1u); set_play(0u);
    AISetStreamVolLeft(1); AISetStreamVolRight(2);
    AISetStreamPlayState(1u);
    dump_probe(out, &off);

    reset_state(0x602u);
    set_rate(0u); set_play(1u);
    AISetStreamVolLeft(3); AISetStreamVolRight(4);
    AISetStreamPlayState(1u);
    dump_probe(out, &off);

    reset_state(0x603u);
    set_rate(0u); set_play(1u);
    AISetStreamVolLeft(5); AISetStreamVolRight(6);
    AISetStreamPlayState(0u);
    dump_probe(out, &off);
}

