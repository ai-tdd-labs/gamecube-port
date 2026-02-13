#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef void (*AIDCallback)(void);

void oracle_AIRegisterDMACallback_reset(u32 s);
AIDCallback oracle_AIRegisterDMACallback(AIDCallback callback);
extern uintptr_t gc_ai_dma_cb_ptr;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

void cb0(void) {}
void cb1(void) {}
void cb2(void) {}

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
    x = rotl1(x) ^ cls(gc_ai_dma_cb_ptr);
    return x;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off, AIDCallback old) {
    be(out + (*off) * 4u, cls((uintptr_t)old)); (*off)++;
    be(out + (*off) * 4u, cls(gc_ai_dma_cb_ptr)); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    oracle_AIRegisterDMACallback_reset(0u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb0)));
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb1)));
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback((AIDCallback)0)));
    oracle_AIRegisterDMACallback_reset(1u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb2)));
    oracle_AIRegisterDMACallback_reset(2u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback((AIDCallback)0)));
    oracle_AIRegisterDMACallback_reset(3u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb0)));
    oracle_AIRegisterDMACallback_reset(4u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb1)));
    oracle_AIRegisterDMACallback_reset(5u);
    a = rotl1(a) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb2)));
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        oracle_AIRegisterDMACallback_reset(0x100u + i);
        AIDCallback cb = (i & 1u) ? cb1 : cb0;
        AIDCallback old = oracle_AIRegisterDMACallback(cb);
        b = rotl1(b) ^ h(cls((uintptr_t)old));
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        oracle_AIRegisterDMACallback_reset(0x200u + i);
        AIDCallback old0 = oracle_AIRegisterDMACallback(cb2);
        AIDCallback old1 = oracle_AIRegisterDMACallback(cb2);
        c = rotl1(c) ^ h(cls((uintptr_t)old0) ^ cls((uintptr_t)old1));
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA1D0C0DEu;
    for (u32 i = 0; i < L3; i++) {
        oracle_AIRegisterDMACallback_reset(rn());
        AIDCallback cb = (rn() & 1u) ? cb0 : (AIDCallback)0;
        AIDCallback old = oracle_AIRegisterDMACallback(cb);
        d = rotl1(d) ^ h(cls((uintptr_t)old));
    }
    tc += L3; m = rotl1(m) ^ d;

    oracle_AIRegisterDMACallback_reset(0x400u);
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb0)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb1)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb2)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback((AIDCallback)0)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb0)));
    e = rotl1(e) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback((AIDCallback)0)));
    tc += L4; m = rotl1(m) ^ e;

    oracle_AIRegisterDMACallback_reset(0x500u);
    f = rotl1(f) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback((AIDCallback)0)));
    f = rotl1(f) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback((AIDCallback)0)));
    f = rotl1(f) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb1)));
    f = rotl1(f) ^ h(cls((uintptr_t)oracle_AIRegisterDMACallback(cb1)));
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x41494342u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    oracle_AIRegisterDMACallback_reset(0x600u);
    dump_probe(out, &off, oracle_AIRegisterDMACallback(cb0));
    dump_probe(out, &off, oracle_AIRegisterDMACallback(cb1));
    dump_probe(out, &off, oracle_AIRegisterDMACallback((AIDCallback)0));
    dump_probe(out, &off, oracle_AIRegisterDMACallback(cb2));

    while (1) { __asm__ volatile("nop"); }
    return 0;
}

