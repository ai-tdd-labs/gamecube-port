#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef void (*ARCallback)(void);

void oracle_ARRegisterDMACallback_reset(u32 s);
ARCallback oracle_ARRegisterDMACallback(ARCallback callback);
extern uintptr_t gc_ar_callback_ptr;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static void cb0(void) {}
static void cb1(void) {}
static void cb2(void) {}

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline u32 cls(uintptr_t p) {
    if (p == 0) return 0u;
    if (p == (uintptr_t)cb0) return 1u;
    if (p == (uintptr_t)cb1) return 2u;
    if (p == (uintptr_t)cb2) return 3u;
    return 4u;
}

static inline u32 h(u32 old_id) {
    u32 x = 0;
    x = rotl1(x) ^ old_id;
    x = rotl1(x) ^ cls(gc_ar_callback_ptr);
    return x;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off, ARCallback old) {
    be(out + (*off) * 4u, cls((uintptr_t)old)); (*off)++;
    be(out + (*off) * 4u, cls(gc_ar_callback_ptr)); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    oracle_ARRegisterDMACallback_reset(0u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb0)));
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb1)));
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback((ARCallback)0)));
    oracle_ARRegisterDMACallback_reset(1u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb2)));
    oracle_ARRegisterDMACallback_reset(2u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback((ARCallback)0)));
    oracle_ARRegisterDMACallback_reset(3u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb0)));
    oracle_ARRegisterDMACallback_reset(4u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb1)));
    oracle_ARRegisterDMACallback_reset(5u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb2)));
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        oracle_ARRegisterDMACallback_reset(0x100u + i);
        ARCallback cb = (i & 1u) ? cb1 : cb0;
        ARCallback old = oracle_ARRegisterDMACallback(cb);
        b = rotl1(b) ^ h(cls((uintptr_t)old));
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        oracle_ARRegisterDMACallback_reset(0x200u + i);
        ARCallback old0 = oracle_ARRegisterDMACallback(cb2);
        ARCallback old1 = oracle_ARRegisterDMACallback(cb2);
        c = rotl1(c) ^ h(cls((uintptr_t)old0) ^ cls((uintptr_t)old1));
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xCAFEBABEu;
    for (u32 i = 0; i < L3; i++) {
        oracle_ARRegisterDMACallback_reset(rn());
        ARCallback cb = (rn() & 1u) ? cb0 : (ARCallback)0;
        ARCallback old = oracle_ARRegisterDMACallback(cb);
        d = rotl1(d) ^ h(cls((uintptr_t)old));
    }
    tc += L3; m = rotl1(m) ^ d;

    oracle_ARRegisterDMACallback_reset(0x400u);
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb0)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb1)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb2)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback((ARCallback)0)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb0)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback((ARCallback)0)));
    tc += L4; m = rotl1(m) ^ e;

    oracle_ARRegisterDMACallback_reset(0x500u);
    f = rotl1(f) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback((ARCallback)0)));
    f = rotl1(f) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback((ARCallback)0)));
    f = rotl1(f) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb1)));
    f = rotl1(f) ^ h(cls((uintptr_t)oracle_ARRegisterDMACallback(cb1)));
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x41524342u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    oracle_ARRegisterDMACallback_reset(0x600u);
    dump_probe(out, &off, oracle_ARRegisterDMACallback(cb0));
    dump_probe(out, &off, oracle_ARRegisterDMACallback(cb1));
    dump_probe(out, &off, oracle_ARRegisterDMACallback((ARCallback)0));
    dump_probe(out, &off, oracle_ARRegisterDMACallback(cb2));

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
