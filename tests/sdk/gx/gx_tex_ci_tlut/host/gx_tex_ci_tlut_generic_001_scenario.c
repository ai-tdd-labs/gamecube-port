#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int16_t  s16;

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

typedef struct {
    u32 tlut;
    u32 loadTlut0;
    u16 numEntries;
    u16 _pad;
} GXTlutObj;

void GXInitTexObjCI(GXTexObj *obj, void *image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t, u8 mipmap, u32 tlut_name);
void GXInitTlutObj(GXTlutObj *tlut_obj, void *lut, u32 fmt, u16 n_entries);
void GXLoadTlut(GXTlutObj *tlut_obj, u32 tlut_name);
void GXSetTexCoordScaleManually(u32 coord, u8 enable, u16 ss, u16 ts);

extern u32 gc_gx_su_ts0[8];
extern u32 gc_gx_su_ts1[8];
extern u32 gc_gx_tcs_man_enab;
extern u32 gc_gx_tlut_load0_last;
extern u32 gc_gx_tlut_load1_last;

const char *gc_scenario_label(void) { return "GXTexCITlut/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_tex_ci_tlut_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    // Test GXInitTlutObj: init with lut at 0x80100000, format=1 (IA8), 256 entries
    GXTlutObj tobj;
    GXInitTlutObj(&tobj, (void *)0x80100000u, 1, 256);

    // Test GXLoadTlut: load to tlut_name=3
    GXLoadTlut(&tobj, 3);

    // Test GXInitTexObjCI: CI8 format=9, 64x64, wrap clamp/clamp, no mipmap, tlut_name=3
    GXTexObj texobj;
    GXInitTexObjCI(&texobj, (void *)0x80200000u, 64, 64, 9, 0, 0, 0, 3);

    // Test GXSetTexCoordScaleManually: coord=2, enable=1, ss=128, ts=256
    GXSetTexCoordScaleManually(2, 1, 128, 256);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x40; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    wr32be(p + off, 0xDEADBEEFu); off += 4;

    // GXInitTlutObj results
    wr32be(p + off, tobj.tlut); off += 4;
    wr32be(p + off, tobj.loadTlut0); off += 4;
    wr32be(p + off, (u32)tobj.numEntries); off += 4;

    // GXLoadTlut results
    wr32be(p + off, gc_gx_tlut_load0_last); off += 4;
    wr32be(p + off, gc_gx_tlut_load1_last); off += 4;

    // GXInitTexObjCI results
    wr32be(p + off, (u32)texobj.flags); off += 4;
    wr32be(p + off, texobj.tlutName); off += 4;
    wr32be(p + off, texobj.mode0); off += 4;
    wr32be(p + off, texobj.image0); off += 4;
    wr32be(p + off, texobj.fmt); off += 4;

    // GXSetTexCoordScaleManually results
    wr32be(p + off, gc_gx_su_ts0[2]); off += 4;
    wr32be(p + off, gc_gx_su_ts1[2]); off += 4;
    wr32be(p + off, gc_gx_tcs_man_enab); off += 4;
}
