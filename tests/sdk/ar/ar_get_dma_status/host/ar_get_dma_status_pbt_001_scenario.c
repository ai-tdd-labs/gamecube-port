#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

u32 ARGetDMAStatus(void);
extern u32 gc_ar_dma_status;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 8, L5 = 4 };

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline void reset_state(u32 s) {
    gc_ar_dma_status = 0xA5A50000u ^ s;
}

static inline u32 h(u32 ret) {
    u32 x = 0;
    x = rotl1(x) ^ gc_ar_dma_status;
    x = rotl1(x) ^ ret;
    return x;
}

static inline void dump_probe(uint8_t *out, u32 *off, u32 ret) {
    wr32be(out + (*off) * 4u, gc_ar_dma_status); (*off)++;
    wr32be(out + (*off) * 4u, ret); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "ARGetDMAStatus/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/ar_get_dma_status_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x54u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    for (u32 i = 0; i < L0; i++) {
        reset_state(i);
        u32 r = ARGetDMAStatus();
        a = rotl1(a) ^ h(r);
    }
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        u32 r0 = ARGetDMAStatus();
        u32 r1 = ARGetDMAStatus();
        b = rotl1(b) ^ h(r0 ^ r1);
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        u32 r0 = ARGetDMAStatus();
        reset_state(0x200u + i);
        u32 r1 = ARGetDMAStatus();
        c = rotl1(c) ^ h(r0 ^ r1);
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xD15EA5Eu;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        u32 r = ARGetDMAStatus();
        d = rotl1(d) ^ h(r);
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0u); e = rotl1(e) ^ h(ARGetDMAStatus());
    reset_state(1u); e = rotl1(e) ^ h(ARGetDMAStatus());
    reset_state(0xFFFFFFFFu); e = rotl1(e) ^ h(ARGetDMAStatus());
    reset_state(0x80000000u); e = rotl1(e) ^ h(ARGetDMAStatus());
    reset_state(0x7FFFFFFFu); e = rotl1(e) ^ h(ARGetDMAStatus());
    reset_state(0x00000200u); e = rotl1(e) ^ h(ARGetDMAStatus());
    reset_state(0x00000000u); e = rotl1(e) ^ h(ARGetDMAStatus());
    reset_state(0x00000200u); e = rotl1(e) ^ h(ARGetDMAStatus());
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x12345678u);
    u32 r = ARGetDMAStatus();
    f = rotl1(f) ^ h(r);
    r = ARGetDMAStatus();
    f = rotl1(f) ^ h(r);
    reset_state(0x12345678u);
    r = ARGetDMAStatus();
    f = rotl1(f) ^ h(r);
    reset_state(0x00000000u);
    r = ARGetDMAStatus();
    f = rotl1(f) ^ h(r);
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x41524453u); off++;
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    reset_state(0x00000200u);
    dump_probe(out, &off, ARGetDMAStatus());
    reset_state(0x00000000u);
    dump_probe(out, &off, ARGetDMAStatus());
    reset_state(0xFFFFFFFFu);
    dump_probe(out, &off, ARGetDMAStatus());
    reset_state(0x7FFFFFFFu);
    dump_probe(out, &off, ARGetDMAStatus());
}
