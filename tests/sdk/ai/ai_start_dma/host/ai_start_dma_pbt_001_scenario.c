#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

void AIStartDMA(void);
extern u16 gc_ai_dsp_regs[4];

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

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

static inline void dump_probe(uint8_t *out, u32 *off) {
    wr32be(out + (*off) * 4u, (u32)gc_ai_dsp_regs[3]); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "AIStartDMA/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/ai_start_dma_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    reset_state(0u);
    gc_ai_dsp_regs[3] = 0x0000u; AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(1u);
    gc_ai_dsp_regs[3] = 0x7FFFu; AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(2u);
    gc_ai_dsp_regs[3] = 0x8000u; AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(3u);
    gc_ai_dsp_regs[3] = 0xFFFFu; AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(4u);
    gc_ai_dsp_regs[3] = 0x1234u; AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(5u);
    gc_ai_dsp_regs[3] = 0x8001u; AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(6u);
    gc_ai_dsp_regs[3] = 0x0001u; AIStartDMA(); a = rotl1(a) ^ h();
    reset_state(7u);
    gc_ai_dsp_regs[3] = 0x4000u; AIStartDMA(); a = rotl1(a) ^ h();
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        gc_ai_dsp_regs[3] = (u16)(i * 0x1111u);
        AIStartDMA();
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        gc_ai_dsp_regs[3] = (u16)(0x1234u ^ (u16)i);
        AIStartDMA();
        AIStartDMA();
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA15D7A27u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        gc_ai_dsp_regs[3] = (u16)rn();
        AIStartDMA();
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    gc_ai_dsp_regs[3] = 0u; AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 1u; AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 0x7FFFu; AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 0x8000u; AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 0x8001u; AIStartDMA(); e = rotl1(e) ^ h();
    gc_ai_dsp_regs[3] = 0xFFFFu; AIStartDMA(); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    gc_ai_dsp_regs[3] = 0u; AIStartDMA(); f = rotl1(f) ^ h();
    AIStartDMA(); f = rotl1(f) ^ h();
    gc_ai_dsp_regs[3] = 0x7FFFu; AIStartDMA(); f = rotl1(f) ^ h();
    AIStartDMA(); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x41495344u); off++; /* AISD */
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    reset_state(0x600u); gc_ai_dsp_regs[3] = 0x0000u; AIStartDMA(); dump_probe(out, &off);
    reset_state(0x601u); gc_ai_dsp_regs[3] = 0x7FFFu; AIStartDMA(); dump_probe(out, &off);
    reset_state(0x602u); gc_ai_dsp_regs[3] = 0x8000u; AIStartDMA(); dump_probe(out, &off);
    reset_state(0x603u); gc_ai_dsp_regs[3] = 0xFFFFu; AIStartDMA(); dump_probe(out, &off);
}

