#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

void AIInitDMA(u32 addr, u32 length);
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
    x = rotl1(x) ^ (u32)gc_ai_dsp_regs[0];
    x = rotl1(x) ^ (u32)gc_ai_dsp_regs[1];
    x = rotl1(x) ^ (u32)gc_ai_dsp_regs[3];
    return x;
}

static inline void dump_probe(uint8_t *out, u32 *off) {
    wr32be(out + (*off) * 4u, (u32)gc_ai_dsp_regs[0]); (*off)++;
    wr32be(out + (*off) * 4u, (u32)gc_ai_dsp_regs[1]); (*off)++;
    wr32be(out + (*off) * 4u, (u32)gc_ai_dsp_regs[3]); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "AIInitDMA/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/ai_init_dma_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x80u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    /* L0: preserve reg24 high bits + reg27 start bit; set addr+len fields. */
    reset_state(0u);
    gc_ai_dsp_regs[0] = 0xFC00u; gc_ai_dsp_regs[3] = 0x8000u;
    AIInitDMA(0x12345678u, 0x00000020u); a = rotl1(a) ^ h();
    reset_state(1u);
    gc_ai_dsp_regs[0] = 0x0000u; gc_ai_dsp_regs[3] = 0x0000u;
    AIInitDMA(0x00000000u, 0x00000000u); a = rotl1(a) ^ h();
    reset_state(2u);
    gc_ai_dsp_regs[0] = 0xAAAAu; gc_ai_dsp_regs[3] = 0x8001u;
    AIInitDMA(0xFFFFFFFFu, 0x00000100u); a = rotl1(a) ^ h();
    reset_state(3u);
    gc_ai_dsp_regs[0] = 0x5555u; gc_ai_dsp_regs[3] = 0x0001u;
    AIInitDMA(0x00ABCDEFu, 0x00000200u); a = rotl1(a) ^ h();
    reset_state(4u);
    gc_ai_dsp_regs[0] = 0xF000u; gc_ai_dsp_regs[3] = 0x7FFFu;
    AIInitDMA(0x03FF001Fu, 0x00000400u); a = rotl1(a) ^ h();
    reset_state(5u);
    gc_ai_dsp_regs[0] = 0x0F00u; gc_ai_dsp_regs[3] = 0x8000u;
    AIInitDMA(0x04000020u, 0x00000800u); a = rotl1(a) ^ h();
    reset_state(6u);
    gc_ai_dsp_regs[0] = 0xFC00u; gc_ai_dsp_regs[3] = 0x0000u;
    AIInitDMA(0xDEADBEEFu, 0x00000020u); a = rotl1(a) ^ h();
    reset_state(7u);
    gc_ai_dsp_regs[0] = 0x8000u; gc_ai_dsp_regs[3] = 0x8000u;
    AIInitDMA(0xCAFEBABEu, 0x00000040u); a = rotl1(a) ^ h();
    tc += L0; m = rotl1(m) ^ a;

    rs = 0x1E17DAD1u;
    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        /* Keep start-bit flipping in reg27, vary length aligned to 32. */
        gc_ai_dsp_regs[0] = (u16)(rn() | 0xFC00u);
        gc_ai_dsp_regs[3] = (u16)((rn() & 0x8000u) | (gc_ai_dsp_regs[3] & 0x7FFFu));
        u32 addr = rn();
        u32 len = (rn() & 0x3FFu) << 5; /* multiple of 32 */
        AIInitDMA(addr, len);
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        u32 addr = 0x80000000u + i * 0x20u;
        u32 len = 0x20u << (i & 3u);
        AIInitDMA(addr, len);
        AIInitDMA(addr, len);
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA1D00D00u;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        u32 addr = rn();
        u32 len = (rn() & 0x7FFu) << 5;
        AIInitDMA(addr, len);
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    AIInitDMA(0x80000000u, 0x00000020u); e = rotl1(e) ^ h();
    AIInitDMA(0x80000020u, 0x00000040u); e = rotl1(e) ^ h();
    AIInitDMA(0x80001000u, 0x00000100u); e = rotl1(e) ^ h();
    AIInitDMA(0x80002000u, 0x00000200u); e = rotl1(e) ^ h();
    AIInitDMA(0x80003000u, 0x00000400u); e = rotl1(e) ^ h();
    AIInitDMA(0x80004000u, 0x00000800u); e = rotl1(e) ^ h();
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    AIInitDMA(0u, 0u); f = rotl1(f) ^ h();
    AIInitDMA(0xFFFFFFFFu, 0x00000020u); f = rotl1(f) ^ h();
    AIInitDMA(0x0000001Fu, 0x00000020u); f = rotl1(f) ^ h();
    AIInitDMA(0x03FF0000u, 0x0000FFE0u); f = rotl1(f) ^ h();
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x41494944u); off++; /* AIID */
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    reset_state(0x600u); gc_ai_dsp_regs[0] = 0xFC00u; gc_ai_dsp_regs[3] = 0x8000u;
    AIInitDMA(0x12345678u, 0x00000020u); dump_probe(out, &off);
    reset_state(0x601u); gc_ai_dsp_regs[0] = 0x8000u; gc_ai_dsp_regs[3] = 0x0000u;
    AIInitDMA(0x0000001Fu, 0x00000800u); dump_probe(out, &off);
    reset_state(0x602u); gc_ai_dsp_regs[0] = 0xAAAAu; gc_ai_dsp_regs[3] = 0x8000u;
    AIInitDMA(0xDEADBEEFu, 0x00000100u); dump_probe(out, &off);
    reset_state(0x603u); gc_ai_dsp_regs[0] = 0x5555u; gc_ai_dsp_regs[3] = 0x8001u;
    AIInitDMA(0xCAFEBABEu, 0x00000040u); dump_probe(out, &off);
}
