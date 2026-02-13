#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef void (*ARCallback)(void);

void oracle_ARSetSize(void);

extern u32 gc_ar_dma_type;
extern u32 gc_ar_dma_mainmem;
extern u32 gc_ar_dma_aram;
extern u32 gc_ar_dma_length;
extern u32 gc_ar_dma_status;
extern uintptr_t gc_ar_callback_ptr;

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

static inline void dump_probe(volatile uint8_t *out, u32 *off) {
    be(out + (*off) * 4u, gc_ar_dma_type); (*off)++;
    be(out + (*off) * 4u, gc_ar_dma_mainmem); (*off)++;
    be(out + (*off) * 4u, gc_ar_dma_aram); (*off)++;
    be(out + (*off) * 4u, gc_ar_dma_length); (*off)++;
    be(out + (*off) * 4u, gc_ar_dma_status); (*off)++;
    be(out + (*off) * 4u, cls(gc_ar_callback_ptr)); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

    for (u32 i = 0; i < L0; i++) {
        reset_state(i);
        oracle_ARSetSize();
        a = rotl1(a) ^ h();
    }
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        reset_state(0x100u + i);
        oracle_ARSetSize();
        b = rotl1(b) ^ h();
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        reset_state(0x200u + i);
        oracle_ARSetSize();
        oracle_ARSetSize();
        c = rotl1(c) ^ h();
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0xA11DAB1Eu;
    for (u32 i = 0; i < L3; i++) {
        reset_state(rn());
        oracle_ARSetSize();
        d = rotl1(d) ^ h();
    }
    tc += L3; m = rotl1(m) ^ d;

    reset_state(0x400u);
    for (u32 i = 0; i < L4; i++) {
        oracle_ARSetSize();
        e = rotl1(e) ^ h();
        gc_ar_dma_status ^= 0x01010101u;
    }
    tc += L4; m = rotl1(m) ^ e;

    reset_state(0x500u);
    gc_ar_dma_type = 0xFFFFFFFFu;
    gc_ar_dma_mainmem = 0u;
    gc_ar_dma_aram = 0xFFFFFFFFu;
    gc_ar_dma_length = 0u;
    gc_ar_dma_status = 0xFFFFFFFFu;
    gc_ar_callback_ptr = (uintptr_t)0;
    oracle_ARSetSize(); f = rotl1(f) ^ h();

    gc_ar_dma_type = 0u;
    gc_ar_dma_mainmem = 0xFFFFFFFFu;
    gc_ar_dma_aram = 0u;
    gc_ar_dma_length = 0xFFFFFFFFu;
    gc_ar_dma_status = 0u;
    gc_ar_callback_ptr = (uintptr_t)cb2;
    oracle_ARSetSize(); f = rotl1(f) ^ h();

    gc_ar_dma_type = 0u;
    gc_ar_dma_mainmem = 0u;
    gc_ar_dma_aram = 0u;
    gc_ar_dma_length = 0u;
    gc_ar_dma_status = 0u;
    gc_ar_callback_ptr = (uintptr_t)0;
    oracle_ARSetSize(); f = rotl1(f) ^ h();

    gc_ar_dma_type = 1u;
    gc_ar_dma_mainmem = 1u;
    gc_ar_dma_aram = 1u;
    gc_ar_dma_length = 1u;
    gc_ar_dma_status = 1u;
    gc_ar_callback_ptr = (uintptr_t)cb0;
    oracle_ARSetSize(); f = rotl1(f) ^ h();

    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x41525353u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    reset_state(0x600u); oracle_ARSetSize(); dump_probe(out, &off);
    reset_state(0x601u); oracle_ARSetSize(); dump_probe(out, &off);
    reset_state(0x602u); oracle_ARSetSize(); dump_probe(out, &off);
    reset_state(0x603u); oracle_ARSetSize(); dump_probe(out, &off);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}

