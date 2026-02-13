#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef void (*AIDCallback)(void);

AIDCallback AIRegisterDMACallback(AIDCallback callback);
extern uintptr_t gc_ai_dma_cb_ptr;

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
    gc_ai_dma_cb_ptr = (uintptr_t)(0x80000000u | (s & 0x00FFFFFFu));
}

static inline u32 h(u32 old_id) {
    u32 x = 0;
    x = rotl1(x) ^ old_id;
    x = rotl1(x) ^ cls(gc_ai_dma_cb_ptr);
    return x;
}

static inline void dump_probe(uint8_t *out, u32 *off, AIDCallback old) {
    wr32be(out + (*off) * 4u, cls((uintptr_t)old)); (*off)++;
    wr32be(out + (*off) * 4u, cls(gc_ai_dma_cb_ptr)); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "AIRegisterDMACallback/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/ai_register_dma_callback_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x60u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    reset_state(0u);
    a = rotl1(a) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb0)));
    a = rotl1(a) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb1)));
    a = rotl1(a) ^ h(cls((uintptr_t)AIRegisterDMACallback((AIDCallback)0)));
    reset_state(1u);
    a = rotl1(a) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb2)));
    reset_state(2u);
    a = rotl1(a) ^ h(cls((uintptr_t)AIRegisterDMACallback((AIDCallback)0)));
    reset_state(3u);
    a = rotl1(a) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb0)));
    reset_state(4u);
    a = rotl1(a) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb1)));
    reset_state(5u);
    a = rotl1(a) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb2)));
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        AIDCallback cb = (i & 1u) ? cb1 : cb0;
        AIDCallback old = AIRegisterDMACallback(cb);
        b = rotl1(b) ^ h(cls((uintptr_t)old));
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        AIDCallback old0 = AIRegisterDMACallback(cb2);
        AIDCallback old1 = AIRegisterDMACallback(cb2);
        c = rotl1(c) ^ h(cls((uintptr_t)old0) ^ cls((uintptr_t)old1));
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA1D0C0DEu;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        AIDCallback cb = (rn() & 1u) ? cb0 : (AIDCallback)0;
        AIDCallback old = AIRegisterDMACallback(cb);
        d = rotl1(d) ^ h(cls((uintptr_t)old));
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    e = rotl1(e) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb0)));
    e = rotl1(e) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb1)));
    e = rotl1(e) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb2)));
    e = rotl1(e) ^ h(cls((uintptr_t)AIRegisterDMACallback((AIDCallback)0)));
    e = rotl1(e) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb0)));
    e = rotl1(e) ^ h(cls((uintptr_t)AIRegisterDMACallback((AIDCallback)0)));
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    f = rotl1(f) ^ h(cls((uintptr_t)AIRegisterDMACallback((AIDCallback)0)));
    f = rotl1(f) ^ h(cls((uintptr_t)AIRegisterDMACallback((AIDCallback)0)));
    f = rotl1(f) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb1)));
    f = rotl1(f) ^ h(cls((uintptr_t)AIRegisterDMACallback(cb1)));
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x41494342u); off++; /* AICB */
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    reset_state(0x600u);
    dump_probe(out, &off, AIRegisterDMACallback(cb0));
    dump_probe(out, &off, AIRegisterDMACallback(cb1));
    dump_probe(out, &off, AIRegisterDMACallback((AIDCallback)0));
    dump_probe(out, &off, AIRegisterDMACallback(cb2));
}

