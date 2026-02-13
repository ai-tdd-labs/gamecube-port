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

void oracle_GXLoadTlut_reset(void);
void oracle_GXLoadTlut_make_obj(GXTlutObj *o, u32 lut_addr, u32 fmt, u16 n_entries, u32 seed);
void oracle_GXLoadTlut(GXTlutObj *tlut_obj, u32 tlut_name);
extern u32 oracle_gx_bp_mask;
extern u32 oracle_gx_last_ras_reg;
extern u32 oracle_gx_tlut_load0_last;
extern u32 oracle_gx_tlut_load1_last;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline u32 hash_obj(const GXTlutObj *o) {
    u32 h = 0;
    h = rotl1(h) ^ o->tlut;
    h = rotl1(h) ^ o->loadTlut0;
    h = rotl1(h) ^ (((u32)o->numEntries << 16) | o->_pad);
    h = rotl1(h) ^ oracle_gx_tlut_load0_last;
    h = rotl1(h) ^ oracle_gx_tlut_load1_last;
    h = rotl1(h) ^ oracle_gx_last_ras_reg;
    h = rotl1(h) ^ oracle_gx_bp_mask;
    return h;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off, const GXTlutObj *o) {
    be(out + (*off) * 4u, o->tlut); (*off)++;
    be(out + (*off) * 4u, o->loadTlut0); (*off)++;
    be(out + (*off) * 4u, ((u32)o->numEntries << 16) | o->_pad); (*off)++;
    be(out + (*off) * 4u, oracle_gx_tlut_load1_last); (*off)++;
    be(out + (*off) * 4u, oracle_gx_last_ras_reg); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    GXTlutObj o;

    oracle_GXLoadTlut_reset();
    for (u32 i = 0; i < L0; i++) {
        oracle_GXLoadTlut_make_obj(&o, 0x80100000u + i * 0x40u, i & 3u, (u16)(16u + i), i);
        oracle_GXLoadTlut(&o, (i < 6u) ? i : (20u + i));
        a = rotl1(a) ^ hash_obj(&o);
    }
    tc += L0; m = rotl1(m) ^ a;

    oracle_GXLoadTlut_reset();
    for (u32 i = 0; i < L1; i++) {
        oracle_GXLoadTlut_make_obj(&o, 0x80200000u + i * 0x80u, (i >> 1) & 3u, (u16)(0x40u + i * 3u), 0x100u + i);
        oracle_GXLoadTlut(&o, i);
        b = rotl1(b) ^ hash_obj(&o);
    }
    tc += L1; m = rotl1(m) ^ b;

    oracle_GXLoadTlut_reset();
    for (u32 i = 0; i < L2; i++) {
        u32 name = (i & 1u) ? (u32)(i % 20u) : (u32)(22u + i);
        oracle_GXLoadTlut_make_obj(&o, 0x80300000u + i * 0x20u, i & 3u, (u16)(0x80u + i), 0x200u + i);
        oracle_GXLoadTlut(&o, name);
        oracle_GXLoadTlut(&o, name);
        c = rotl1(c) ^ hash_obj(&o);
    }
    tc += L2; m = rotl1(m) ^ c;

    oracle_GXLoadTlut_reset();
    rs = 0xBADC0DEu;
    for (u32 i = 0; i < L3; i++) {
        oracle_GXLoadTlut_make_obj(&o, 0x80400000u + (rn() & 0x7FFFu), rn() & 3u, (u16)rn(), rn());
        oracle_GXLoadTlut(&o, rn() % 28u);
        d = rotl1(d) ^ hash_obj(&o);
    }
    tc += L3; m = rotl1(m) ^ d;

    oracle_GXLoadTlut_reset();
    oracle_GXLoadTlut_make_obj(&o, 0x80500000u, 2u, 256u, 0x400u); oracle_GXLoadTlut(&o, 0u); e = rotl1(e) ^ hash_obj(&o);
    oracle_GXLoadTlut_make_obj(&o, 0x80501000u, 2u, 512u, 0x401u); oracle_GXLoadTlut(&o, 3u); e = rotl1(e) ^ hash_obj(&o);
    oracle_GXLoadTlut_make_obj(&o, 0x80502000u, 1u, 128u, 0x402u); oracle_GXLoadTlut(&o, 7u); e = rotl1(e) ^ hash_obj(&o);
    oracle_GXLoadTlut_make_obj(&o, 0x80503000u, 1u, 64u, 0x403u); oracle_GXLoadTlut(&o, 0u); e = rotl1(e) ^ hash_obj(&o);
    oracle_GXLoadTlut_make_obj(&o, 0x80504000u, 0u, 32u, 0x404u); oracle_GXLoadTlut(&o, 17u); e = rotl1(e) ^ hash_obj(&o);
    oracle_GXLoadTlut_make_obj(&o, 0x80505000u, 3u, 16u, 0x405u); oracle_GXLoadTlut(&o, 25u); e = rotl1(e) ^ hash_obj(&o);
    tc += L4; m = rotl1(m) ^ e;

    oracle_GXLoadTlut_reset();
    oracle_GXLoadTlut_make_obj(&o, 0x80000000u, 0u, 0u, 0x500u); oracle_GXLoadTlut(&o, 0u); f = rotl1(f) ^ hash_obj(&o);
    oracle_GXLoadTlut_make_obj(&o, 0x817FFFFFu, 3u, 0xFFFFu, 0x501u); oracle_GXLoadTlut(&o, 19u); f = rotl1(f) ^ hash_obj(&o);
    oracle_GXLoadTlut_make_obj(&o, 0x8030001Fu, 2u, 1u, 0x502u); oracle_GXLoadTlut(&o, 20u); f = rotl1(f) ^ hash_obj(&o);
    oracle_GXLoadTlut_make_obj(&o, 0x80300020u, 1u, 2u, 0x503u); oracle_GXLoadTlut(&o, 999u); f = rotl1(f) ^ hash_obj(&o);
    tc += L5; m = rotl1(m) ^ f;

    be(out + off * 4u, 0x474C5431u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, m); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    oracle_GXLoadTlut_reset();
    oracle_GXLoadTlut_make_obj(&o, 0x80B00000u, 2u, 256u, 0x600u); oracle_GXLoadTlut(&o, 3u); dump_probe(out, &off, &o);
    oracle_GXLoadTlut_make_obj(&o, 0x80B01000u, 1u, 128u, 0x601u); oracle_GXLoadTlut(&o, 19u); dump_probe(out, &off, &o);
    oracle_GXLoadTlut_make_obj(&o, 0x80B02000u, 3u, 64u, 0x602u); oracle_GXLoadTlut(&o, 20u); dump_probe(out, &off, &o);
    oracle_GXLoadTlut_make_obj(&o, 0x80B03000u, 0u, 32u, 0x603u); oracle_GXLoadTlut(&o, 999u); dump_probe(out, &off, &o);

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
