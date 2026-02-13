#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct {
    u32 tlut;
    u32 loadTlut0;
    u16 numEntries;
    u16 _pad;
} GXTlutObj;

typedef struct {
    u32 loadTlut1;
    GXTlutObj tlutObj;
} GXTlutRegion;

u32 oracle_gx_bp_mask;
u32 oracle_gx_last_ras_reg;
u32 oracle_gx_tlut_load0_last;
u32 oracle_gx_tlut_load1_last;

static GXTlutRegion oracle_regions[20];
static const u16 oracle_tlut_size_table[10] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 };

static inline u32 set_field(u32 r, u32 n, u32 s, u32 v) {
    u32 m = ((1u << n) - 1u) << s;
    return (r & ~m) | ((v << s) & m);
}

static inline void write_ras(u32 v) {
    oracle_gx_last_ras_reg = v;
}

static void init_tlut_region(GXTlutRegion *region, u32 tmem_addr, u8 tlut_sz) {
    if (!region) return;
    if (tlut_sz > 9u) tlut_sz = 9u;
    region->loadTlut1 = 0;
    region->loadTlut1 = set_field(region->loadTlut1, 10, 0, tmem_addr >> 9);
    region->loadTlut1 = set_field(region->loadTlut1, 11, 10, (u32)oracle_tlut_size_table[tlut_sz]);
    region->loadTlut1 = set_field(region->loadTlut1, 8, 24, 0x65u);
}

void oracle_GXLoadTlut_reset(void) {
    u32 i;
    oracle_gx_bp_mask = 0xFFu;
    oracle_gx_last_ras_reg = 0;
    oracle_gx_tlut_load0_last = 0;
    oracle_gx_tlut_load1_last = 0;

    for (i = 0; i < 16u; i++) {
        init_tlut_region(&oracle_regions[i], 0xC0000u + 0x2000u * i, 4);
    }
    for (i = 0; i < 4u; i++) {
        init_tlut_region(&oracle_regions[16u + i], 0xE0000u + 0x8000u * i, 9);
    }
}

void oracle_GXLoadTlut_make_obj(GXTlutObj *o, u32 lut_addr, u32 fmt, u16 n_entries, u32 seed) {
    if (!o) return;
    o->tlut = 0xA5A00000u ^ seed;
    o->tlut = set_field(o->tlut, 2, 10, fmt & 3u);
    o->tlut = set_field(o->tlut, 10, 0, seed & 0x3FFu);
    o->loadTlut0 = 0;
    o->loadTlut0 = set_field(o->loadTlut0, 21, 0, (lut_addr & 0x3FFFFFFFu) >> 5);
    o->loadTlut0 = set_field(o->loadTlut0, 8, 24, 0x64u);
    o->numEntries = n_entries;
    o->_pad = (u16)(0x5A00u ^ (u16)seed);
}

void oracle_GXLoadTlut(GXTlutObj *tlut_obj, u32 tlut_name) {
    GXTlutRegion *r;
    u32 tlut_offset;

    if (!tlut_obj) return;
    r = (tlut_name >= 20u) ? &oracle_regions[0] : &oracle_regions[tlut_name];

    write_ras(oracle_gx_bp_mask);
    write_ras(tlut_obj->loadTlut0);
    write_ras(r->loadTlut1);
    write_ras(oracle_gx_bp_mask);

    oracle_gx_tlut_load0_last = tlut_obj->loadTlut0;
    oracle_gx_tlut_load1_last = r->loadTlut1;

    tlut_offset = r->loadTlut1 & 0x3FFu;
    tlut_obj->tlut = set_field(tlut_obj->tlut, 10, 0, tlut_offset);
    r->tlutObj = *tlut_obj;
}
