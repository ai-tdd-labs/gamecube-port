#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

typedef struct {
    u32 tlut;
    u32 loadTlut0;
    u16 numEntries;
    u16 _pad;
} GXTlutObj;

void *GXInit(void *base, u32 size);
void GXLoadTlut(GXTlutObj *tlut_obj, u32 tlut_name);

extern u32 gc_gx_bp_mask;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_tlut_load0_last;
extern u32 gc_gx_tlut_load1_last;

enum { L0 = 8, L1 = 16, L2 = 8, L3 = 2048, L4 = 6, L5 = 4 };

static inline u32 set_field(u32 r, u32 n, u32 s, u32 v) {
    u32 m = ((1u << n) - 1u) << s;
    return (r & ~m) | ((v << s) & m);
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline void make_obj(GXTlutObj *o, u32 lut_addr, u32 fmt, u16 n_entries, u32 seed) {
    o->tlut = 0xA5A00000u ^ seed;
    o->tlut = set_field(o->tlut, 2, 10, fmt & 3u);
    o->tlut = set_field(o->tlut, 10, 0, seed & 0x3FFu);
    o->loadTlut0 = 0;
    o->loadTlut0 = set_field(o->loadTlut0, 21, 0, (lut_addr & 0x3FFFFFFFu) >> 5);
    o->loadTlut0 = set_field(o->loadTlut0, 8, 24, 0x64u);
    o->numEntries = n_entries;
    o->_pad = (u16)(0x5A00u ^ (u16)seed);
}

static inline u32 hash_obj(const GXTlutObj *o) {
    u32 h = 0;
    h = rotl1(h) ^ o->tlut;
    h = rotl1(h) ^ o->loadTlut0;
    h = rotl1(h) ^ (((u32)o->numEntries << 16) | o->_pad);
    h = rotl1(h) ^ gc_gx_tlut_load0_last;
    h = rotl1(h) ^ gc_gx_tlut_load1_last;
    h = rotl1(h) ^ gc_gx_last_ras_reg;
    h = rotl1(h) ^ gc_gx_bp_mask;
    return h;
}

static inline void dump_probe(uint8_t *out, u32 *off, const GXTlutObj *o) {
    wr32be(out + (*off) * 4u, o->tlut); (*off)++;
    wr32be(out + (*off) * 4u, o->loadTlut0); (*off)++;
    wr32be(out + (*off) * 4u, ((u32)o->numEntries << 16) | o->_pad); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_tlut_load1_last); (*off)++;
    wr32be(out + (*off) * 4u, gc_gx_last_ras_reg); (*off)++;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char *gc_scenario_label(void) { return "GXLoadTlut/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_load_tlut_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0xA0u);
    if (!out) die("gc_ram_ptr failed");

    u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    GXTlutObj o;

    GXInit(0, 0);
    for (u32 i = 0; i < L0; i++) {
        make_obj(&o, 0x80100000u + i * 0x40u, i & 3u, (u16)(16u + i), i);
        GXLoadTlut(&o, (i < 6u) ? i : (20u + i));
        a = rotl1(a) ^ hash_obj(&o);
    }
    tc += L0; m = rotl1(m) ^ a;

    GXInit(0, 0);
    for (u32 i = 0; i < L1; i++) {
        make_obj(&o, 0x80200000u + i * 0x80u, (i >> 1) & 3u, (u16)(0x40u + i * 3u), 0x100u + i);
        GXLoadTlut(&o, i);
        b = rotl1(b) ^ hash_obj(&o);
    }
    tc += L1; m = rotl1(m) ^ b;

    GXInit(0, 0);
    for (u32 i = 0; i < L2; i++) {
        u32 name = (i & 1u) ? (u32)(i % 20u) : (u32)(22u + i);
        make_obj(&o, 0x80300000u + i * 0x20u, i & 3u, (u16)(0x80u + i), 0x200u + i);
        GXLoadTlut(&o, name);
        GXLoadTlut(&o, name);
        c = rotl1(c) ^ hash_obj(&o);
    }
    tc += L2; m = rotl1(m) ^ c;

    GXInit(0, 0);
    rs = 0xBADC0DEu;
    for (u32 i = 0; i < L3; i++) {
        make_obj(&o, 0x80400000u + (rn() & 0x7FFFu), rn() & 3u, (u16)rn(), rn());
        GXLoadTlut(&o, rn() % 28u);
        d = rotl1(d) ^ hash_obj(&o);
    }
    tc += L3; m = rotl1(m) ^ d;

    GXInit(0, 0);
    make_obj(&o, 0x80500000u, 2u, 256u, 0x400u); GXLoadTlut(&o, 0u); e = rotl1(e) ^ hash_obj(&o);
    make_obj(&o, 0x80501000u, 2u, 512u, 0x401u); GXLoadTlut(&o, 3u); e = rotl1(e) ^ hash_obj(&o);
    make_obj(&o, 0x80502000u, 1u, 128u, 0x402u); GXLoadTlut(&o, 7u); e = rotl1(e) ^ hash_obj(&o);
    make_obj(&o, 0x80503000u, 1u, 64u, 0x403u); GXLoadTlut(&o, 0u); e = rotl1(e) ^ hash_obj(&o);
    make_obj(&o, 0x80504000u, 0u, 32u, 0x404u); GXLoadTlut(&o, 17u); e = rotl1(e) ^ hash_obj(&o);
    make_obj(&o, 0x80505000u, 3u, 16u, 0x405u); GXLoadTlut(&o, 25u); e = rotl1(e) ^ hash_obj(&o);
    tc += L4; m = rotl1(m) ^ e;

    GXInit(0, 0);
    make_obj(&o, 0x80000000u, 0u, 0u, 0x500u); GXLoadTlut(&o, 0u); f = rotl1(f) ^ hash_obj(&o);
    make_obj(&o, 0x817FFFFFu, 3u, 0xFFFFu, 0x501u); GXLoadTlut(&o, 19u); f = rotl1(f) ^ hash_obj(&o);
    make_obj(&o, 0x8030001Fu, 2u, 1u, 0x502u); GXLoadTlut(&o, 20u); f = rotl1(f) ^ hash_obj(&o);
    make_obj(&o, 0x80300020u, 1u, 2u, 0x503u); GXLoadTlut(&o, 999u); f = rotl1(f) ^ hash_obj(&o);
    tc += L5; m = rotl1(m) ^ f;

    wr32be(out + off * 4u, 0x474C5431u); off++;
    wr32be(out + off * 4u, tc); off++;
    wr32be(out + off * 4u, m); off++;
    wr32be(out + off * 4u, a); off++; wr32be(out + off * 4u, L0); off++;
    wr32be(out + off * 4u, b); off++; wr32be(out + off * 4u, L1); off++;
    wr32be(out + off * 4u, c); off++; wr32be(out + off * 4u, L2); off++;
    wr32be(out + off * 4u, d); off++; wr32be(out + off * 4u, L3); off++;
    wr32be(out + off * 4u, e); off++; wr32be(out + off * 4u, L4); off++;
    wr32be(out + off * 4u, f); off++; wr32be(out + off * 4u, L5); off++;

    GXInit(0, 0);
    make_obj(&o, 0x80B00000u, 2u, 256u, 0x600u); GXLoadTlut(&o, 3u); dump_probe(out, &off, &o);
    make_obj(&o, 0x80B01000u, 1u, 128u, 0x601u); GXLoadTlut(&o, 19u); dump_probe(out, &off, &o);
    make_obj(&o, 0x80B02000u, 3u, 64u, 0x602u); GXLoadTlut(&o, 20u); dump_probe(out, &off, &o);
    make_obj(&o, 0x80B03000u, 0u, 32u, 0x603u); GXLoadTlut(&o, 999u); dump_probe(out, &off, &o);
}
