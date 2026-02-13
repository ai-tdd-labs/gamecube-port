#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef void (*ARCallback)(void);

void ARSetSize(void);

extern u32 gc_ar_dma_type;
extern u32 gc_ar_dma_mainmem;
extern u32 gc_ar_dma_aram;
extern u32 gc_ar_dma_length;
extern u32 gc_ar_dma_status;
extern uintptr_t gc_ar_callback_ptr;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static void cb0(void) {}
static void cb1(void) {}
static void cb2(void) {}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline u32 cls(uintptr_t p) {
    if (p == 0) return 0u;
    if (p == (uintptr_t)cb0) return 1u;
    if (p == (uintptr_t)cb1) return 2u;
    if (p == (uintptr_t)cb2) return 3u;
    return 4u;
}

static inline void reset_state(u32 s) {
    gc_ar_dma_type = 0x10000000u ^ s;
    gc_ar_dma_mainmem = 0x20000000u ^ (s * 3u);
    gc_ar_dma_aram = 0x30000000u ^ (s * 5u);
    gc_ar_dma_length = 0x40000000u ^ (s * 7u);
    gc_ar_dma_status = 0x50000000u ^ (s * 11u);

    switch (s % 4u) {
    case 0: gc_ar_callback_ptr = (uintptr_t)0; break;
    case 1: gc_ar_callback_ptr = (uintptr_t)cb0; break;
    case 2: gc_ar_callback_ptr = (uintptr_t)cb1; break;
    default: gc_ar_callback_ptr = (uintptr_t)cb2; break;
    }
}

static inline u32 h(void) {
    u32 x = 0;
    x = rotl1(x) ^ gc_ar_dma_type;
    x = rotl1(x) ^ gc_ar_dma_mainmem;
    x = rotl1(x) ^ gc_ar_dma_aram;
    x = rotl1(x) ^ gc_ar_dma_length;
    x = rotl1(x) ^ gc_ar_dma_status;
    x = rotl1(x) ^ cls(gc_ar_callback_ptr);
    return x;
}

static inline void dump_probe(uint8_t *out, u32 *off) {
    wr32be(out + (*off) * 4u, gc_ar_dma_type); (*off)++;
    wr32be(out + (*off) * 4u, gc_ar_dma_mainmem); (*off)++;
    wr32be(out + (*off) * 4u, gc_ar_dma_aram); (*off)++;
    wr32be(out + (*off) * 4u, gc_ar_dma_length); (*off)++;
    wr32be(out + (*off) * 4u, gc_ar_dma_status); (*off)++;
    wr32be(out + (*off) * 4u, cls(gc_ar_callback_ptr)); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "ARSetSize/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/ar_set_size_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0xA0u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    for (u32 i = 0; i < L0; i++) {
        reset_state(i);
        ARSetSize();
        a = rotl1(a) ^ h();
    }
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        ARSetSize();
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        ARSetSize();
        ARSetSize();
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA11DAB1Eu;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        ARSetSize();
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    for (u32 i = 0; i < L4; i++) {
        ARSetSize();
        e = rotl1(e) ^ h();
        gc_ar_dma_status ^= 0x01010101u; /* exercise "random start" changes outside ARSetSize */
    }
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    gc_ar_dma_type = 0xFFFFFFFFu;
    gc_ar_dma_mainmem = 0u;
    gc_ar_dma_aram = 0xFFFFFFFFu;
    gc_ar_dma_length = 0u;
    gc_ar_dma_status = 0xFFFFFFFFu;
    gc_ar_callback_ptr = (uintptr_t)0;
    ARSetSize(); f = rotl1(f) ^ h();

    gc_ar_dma_type = 0u;
    gc_ar_dma_mainmem = 0xFFFFFFFFu;
    gc_ar_dma_aram = 0u;
    gc_ar_dma_length = 0xFFFFFFFFu;
    gc_ar_dma_status = 0u;
    gc_ar_callback_ptr = (uintptr_t)cb2;
    ARSetSize(); f = rotl1(f) ^ h();

    gc_ar_dma_type = 0u;
    gc_ar_dma_mainmem = 0u;
    gc_ar_dma_aram = 0u;
    gc_ar_dma_length = 0u;
    gc_ar_dma_status = 0u;
    gc_ar_callback_ptr = (uintptr_t)0;
    ARSetSize(); f = rotl1(f) ^ h();

    gc_ar_dma_type = 1u;
    gc_ar_dma_mainmem = 1u;
    gc_ar_dma_aram = 1u;
    gc_ar_dma_length = 1u;
    gc_ar_dma_status = 1u;
    gc_ar_callback_ptr = (uintptr_t)cb0;
    ARSetSize(); f = rotl1(f) ^ h();

    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x41525353u); off++;
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    reset_state(0x600u); ARSetSize(); dump_probe(out, &off);
    reset_state(0x601u); ARSetSize(); dump_probe(out, &off);
    reset_state(0x602u); ARSetSize(); dump_probe(out, &off);
    reset_state(0x603u); ARSetSize(); dump_probe(out, &off);
}

