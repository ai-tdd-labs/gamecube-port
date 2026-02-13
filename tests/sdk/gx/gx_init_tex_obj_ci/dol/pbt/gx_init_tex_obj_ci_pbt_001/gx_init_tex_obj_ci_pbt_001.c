#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct {
    u32 mode0;
    u32 mode1;
    u32 image0;
    u32 image3;
    u32 userData;
    u32 fmt;
    u32 tlutName;
    u16 loadCnt;
    u8 loadFmt;
    u8 flags;
} GXTexObj;

void oracle_GXInitTexObjCI(
    GXTexObj *obj,
    void *image_ptr,
    u16 width,
    u16 height,
    u32 format,
    u32 wrap_s,
    u32 wrap_t,
    u8 mipmap,
    u32 tlut_name
);

enum {
    L0 = 8,
    L1 = 16,
    L2 = 6,
    L3 = 2048,
    L4 = 6,
    L5 = 4,
};

static inline void be(volatile uint8_t *p, u32 v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) {
    return (v << 1) | (v >> 31);
}

static inline void seed_obj(GXTexObj *o, u32 s) {
    o->mode0 = 0x10000000u ^ s;
    o->mode1 = 0x20000000u ^ rotl1(s);
    o->image0 = 0x30000000u ^ (s * 3u);
    o->image3 = 0x40000000u ^ (s * 5u);
    o->userData = 0x50000000u ^ (s * 7u);
    o->fmt = 0x60000000u ^ (s * 11u);
    o->tlutName = 0x70000000u ^ (s * 13u);
    o->loadCnt = (u16)(0x1234u ^ (u16)s);
    o->loadFmt = (u8)(0x80u ^ (u8)s);
    o->flags = (u8)(0xC0u ^ (u8)(s >> 1));
}

static inline u32 hash_obj(const GXTexObj *o) {
    u32 h = 0;
    h = rotl1(h) ^ o->mode0;
    h = rotl1(h) ^ o->mode1;
    h = rotl1(h) ^ o->image0;
    h = rotl1(h) ^ o->image3;
    h = rotl1(h) ^ o->userData;
    h = rotl1(h) ^ o->fmt;
    h = rotl1(h) ^ o->tlutName;
    h = rotl1(h) ^ (((u32)o->loadCnt << 16) | ((u32)o->loadFmt << 8) | o->flags);
    return h;
}

static inline void dump_probe(volatile uint8_t *out, u32 *off, const GXTexObj *o) {
    be(out + (*off) * 4u, o->mode0); (*off)++;
    be(out + (*off) * 4u, o->image0); (*off)++;
    be(out + (*off) * 4u, o->fmt); (*off)++;
    be(out + (*off) * 4u, o->tlutName); (*off)++;
    be(out + (*off) * 4u, ((u32)o->loadCnt << 16) | ((u32)o->loadFmt << 8) | o->flags); (*off)++;
}

static u32 rs;
static inline u32 rn(void) {
    rs ^= rs << 13;
    rs ^= rs >> 17;
    rs ^= rs << 5;
    return rs;
}

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

    volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
    u32 off = 0;
    u32 tc = 0;
    u32 master = 0;
    u32 a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    GXTexObj o;

    const u32 fmts[3] = {8u, 9u, 10u};

    for (u32 i = 0; i < 8u; i++) {
        seed_obj(&o, i);
        oracle_GXInitTexObjCI(&o, (void *)(uintptr_t)(0x80200000u + i * 0x100u), (u16)(16u + i), (u16)(32u + i),
                              fmts[i % 3u], i % 3u, (i / 3u) % 3u, (u8)(i & 1u), 3u + i);
        a = rotl1(a) ^ hash_obj(&o);
    }
    tc += L0;
    master = rotl1(master) ^ a;

    for (u32 w = 1u; w <= 4u; w++) {
        for (u32 h = 1u; h <= 4u; h++) {
            seed_obj(&o, 0x100u + (w << 4) + h);
            oracle_GXInitTexObjCI(&o, (void *)(uintptr_t)(0x80400000u + (w << 12) + (h << 8)), (u16)(w * 31u), (u16)(h * 29u),
                                  fmts[(w + h) % 3u], (w + h) % 3u, (w * h) % 3u, (u8)((w + h) & 1u), (w << 8) | h);
            b = rotl1(b) ^ hash_obj(&o);
        }
    }
    tc += L1;
    master = rotl1(master) ^ b;

    for (u32 i = 0; i < L2; i++) {
        seed_obj(&o, 0x200u + i);
        u16 ww = (u16)(64u + i * 7u);
        u16 hh = (u16)(48u + i * 5u);
        u32 fmt = fmts[i % 3u];
        u32 ws = (i + 1u) % 3u;
        u32 wt = (i + 2u) % 3u;
        u8 mm = (u8)(i & 1u);
        u32 tn = 0x80u + i;
        oracle_GXInitTexObjCI(&o, (void *)(uintptr_t)(0x80600000u + i * 0x400u), ww, hh, fmt, ws, wt, mm, tn);
        oracle_GXInitTexObjCI(&o, (void *)(uintptr_t)(0x80600000u + i * 0x400u), ww, hh, fmt, ws, wt, mm, tn);
        c = rotl1(c) ^ hash_obj(&o);
    }
    tc += L2;
    master = rotl1(master) ^ c;

    rs = 0xC0DEC0DEu;
    for (u32 i = 0; i < L3; i++) {
        seed_obj(&o, rn());
        u16 ww = (u16)((rn() & 0x01FFu) + 1u);
        u16 hh = (u16)((rn() & 0x01FFu) + 1u);
        u32 fmt = fmts[rn() % 3u];
        u32 ws = rn() % 3u;
        u32 wt = rn() % 3u;
        u8 mm = (u8)(rn() & 1u);
        u32 tn = rn() & 0x3FFu;
        oracle_GXInitTexObjCI(&o, (void *)(uintptr_t)(0x80800000u + (rn() & 0xFFFFu)), ww, hh, fmt, ws, wt, mm, tn);
        d = rotl1(d) ^ hash_obj(&o);
    }
    tc += L3;
    master = rotl1(master) ^ d;

    seed_obj(&o, 0x400u); oracle_GXInitTexObjCI(&o, (void *)0x80280000u, 128, 128, 9u, 0u, 0u, 0u, 0u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x401u); oracle_GXInitTexObjCI(&o, (void *)0x80281000u, 256, 128, 8u, 1u, 1u, 0u, 7u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x402u); oracle_GXInitTexObjCI(&o, (void *)0x80282000u, 64, 64, 10u, 2u, 2u, 1u, 3u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x403u); oracle_GXInitTexObjCI(&o, (void *)0x80283000u, 320, 240, 9u, 1u, 0u, 0u, 0x10u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x404u); oracle_GXInitTexObjCI(&o, (void *)0x80284000u, 640, 480, 8u, 0u, 1u, 0u, 0x11u); e = rotl1(e) ^ hash_obj(&o);
    seed_obj(&o, 0x405u); oracle_GXInitTexObjCI(&o, (void *)0x80285000u, 512, 256, 9u, 2u, 0u, 1u, 0x12u); e = rotl1(e) ^ hash_obj(&o);
    tc += L4;
    master = rotl1(master) ^ e;

    seed_obj(&o, 0x500u); oracle_GXInitTexObjCI(&o, (void *)0x80A00000u, 1, 1, 8u, 0u, 0u, 0u, 0u); f = rotl1(f) ^ hash_obj(&o);
    seed_obj(&o, 0x501u); oracle_GXInitTexObjCI(&o, (void *)0x80A00100u, 1024, 1024, 10u, 2u, 2u, 1u, 0x3FFu); f = rotl1(f) ^ hash_obj(&o);
    seed_obj(&o, 0x502u); oracle_GXInitTexObjCI(&o, (void *)0x80A00200u, 1023, 1023, 9u, 1u, 2u, 0u, 0x7FFFu); f = rotl1(f) ^ hash_obj(&o);
    seed_obj(&o, 0x503u); oracle_GXInitTexObjCI(&o, (void *)0x80A00300u, 17, 19, 8u, 0u, 1u, 0u, 0x1234u); f = rotl1(f) ^ hash_obj(&o);
    tc += L5;
    master = rotl1(master) ^ f;

    be(out + off * 4u, 0x47434931u); off++;
    be(out + off * 4u, tc); off++;
    be(out + off * 4u, master); off++;
    be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
    be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
    be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
    be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
    be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
    be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

    seed_obj(&o, 0x600u); oracle_GXInitTexObjCI(&o, (void *)0x80B00000u, 64, 32, 8u, 0u, 1u, 0u, 0x20u); dump_probe(out, &off, &o);
    seed_obj(&o, 0x601u); oracle_GXInitTexObjCI(&o, (void *)0x80B01000u, 128, 64, 9u, 1u, 2u, 1u, 0x21u); dump_probe(out, &off, &o);
    seed_obj(&o, 0x602u); oracle_GXInitTexObjCI(&o, (void *)0x80B02000u, 256, 128, 10u, 2u, 0u, 0u, 0x22u); dump_probe(out, &off, &o);
    seed_obj(&o, 0x603u); oracle_GXInitTexObjCI(&o, (void *)0x80B03000u, 511, 257, 9u, 1u, 1u, 0u, 0x123u); dump_probe(out, &off, &o);

    while (1) {
        __asm__ volatile("nop");
    }

    return 0;
}
