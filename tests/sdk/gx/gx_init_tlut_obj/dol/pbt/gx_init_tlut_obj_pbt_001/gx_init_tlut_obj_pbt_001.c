#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef uint16_t u16;

typedef struct {
    u32 tlut;
    u32 loadTlut0;
    u16 numEntries;
    u16 _pad;
} GXTlutObj;

void oracle_GXInitTlutObj(GXTlutObj *tlut_obj, void *lut, u32 fmt, u16 n_entries);

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline void seed_obj(GXTlutObj *o, u32 s) {
    o->tlut = 0x11110000u ^ s;
    o->loadTlut0 = 0x22220000u ^ (s * 3u);
    o->numEntries = (u16)(0x3333u ^ (u16)s);
    o->_pad = (u16)(0x4444u ^ (u16)(s * 5u));
}

static inline u32 hash_obj(const GXTlutObj *o) {
    u32 h = 0;
    h = rotl1(h) ^ o->tlut;
    h = rotl1(h) ^ o->loadTlut0;
    h = rotl1(h) ^ ((u32)o->numEntries << 16) ^ o->_pad;
    return h;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off, const GXTlutObj *o) {
    be(out + (*off) * 4u, o->tlut); (*off)++;
    be(out + (*off) * 4u, o->loadTlut0); (*off)++;
    be(out + (*off) * 4u, ((u32)o->numEntries << 16) | o->_pad); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    GXTlutObj o;

    for (u32 i = 0; i < L0; i++) {
        seed_obj(&o, i);
        oracle_GXInitTlutObj(&o, (void *)(uintptr_t)(0x80100000u + i * 0x20u), i & 3u, (u16)(16u + i));
        a = rotl1(a) ^ hash_obj(&o);
    }
    tc += L0; m = rotl1(m) ^ a;

    for (u32 i = 0; i < L1; i++) {
        u32 addr = 0x80200000u + ((i & 3u) << 9) + ((i & 0xCu) << 5);
        seed_obj(&o, 0x100u + i);
        oracle_GXInitTlutObj(&o, (void *)(uintptr_t)addr, (i >> 2) & 3u, (u16)(32u + i * 7u));
        b = rotl1(b) ^ hash_obj(&o);
    }
    tc += L1; m = rotl1(m) ^ b;

    for (u32 i = 0; i < L2; i++) {
        seed_obj(&o, 0x200u + i);
        u32 addr = 0x80400000u + i * 0x100u;
        u32 fmt = i & 3u;
        u16 n = (u16)(0x100u + i * 0x10u);
        oracle_GXInitTlutObj(&o, (void *)(uintptr_t)addr, fmt, n);
        oracle_GXInitTlutObj(&o, (void *)(uintptr_t)addr, fmt, n);
        c = rotl1(c) ^ hash_obj(&o);
    }
    tc += L2; m = rotl1(m) ^ c;

    rs = 0x51A7D00Du;
    for (u32 i = 0; i < L3; i++) {
        seed_obj(&o, rn());
        oracle_GXInitTlutObj(&o, (void *)(uintptr_t)(0x80600000u + (rn() & 0x1FFFFu)), rn() & 3u, (u16)rn());
        d = rotl1(d) ^ hash_obj(&o);
    }
    tc += L3; m = rotl1(m) ^ d;

    seed_obj(&o, 0x400u); oracle_GXInitTlutObj(&o, (void *)0x80110000u, 2u, 256u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x401u); oracle_GXInitTlutObj(&o, (void *)0x80112000u, 2u, 512u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x402u); oracle_GXInitTlutObj(&o, (void *)0x80114000u, 1u, 128u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x403u); oracle_GXInitTlutObj(&o, (void *)0x80116000u, 1u, 64u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x404u); oracle_GXInitTlutObj(&o, (void *)0x80118000u, 0u, 16u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x405u); oracle_GXInitTlutObj(&o, (void *)0x8011A000u, 3u, 1024u); e = rotl1(e) ^ hash_obj(&o);
    tc += L4; m = rotl1(m) ^ e;

    seed_obj(&o, 0x500u); oracle_GXInitTlutObj(&o, (void *)0x80000000u, 0u, 0u); f = rotl1(f) ^ hash_obj(&o);
    seed_obj(&o, 0x501u); oracle_GXInitTlutObj(&o, (void *)0x817FFFFFu, 3u, 0xFFFFu); f = rotl1(f) ^ hash_obj(&o);
    seed_obj(&o, 0x502u); oracle_GXInitTlutObj(&o, (void *)0x8030001Fu, 2u, 1u); f = rotl1(f) ^ hash_obj(&o);
    seed_obj(&o, 0x503u); oracle_GXInitTlutObj(&o, (void *)0x80300020u, 1u, 2u); f = rotl1(f) ^ hash_obj(&o);
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x47544C31u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    seed_obj(&o, 0x600u); oracle_GXInitTlutObj(&o, (void *)0x80B00000u, 2u, 256u); dump_probe(out, &off, &o);
    seed_obj(&o, 0x601u); oracle_GXInitTlutObj(&o, (void *)0x80B01000u, 1u, 128u); dump_probe(out, &off, &o);
    seed_obj(&o, 0x602u); oracle_GXInitTlutObj(&o, (void *)0x80B02000u, 3u, 64u); dump_probe(out, &off, &o);
    seed_obj(&o, 0x603u); oracle_GXInitTlutObj(&o, (void *)0x80B03000u, 0u, 32u); dump_probe(out, &off, &o);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
