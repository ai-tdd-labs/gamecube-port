#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

enum { GX_CTF_R8 = 0x4 };

void GXSetTexCopyDst(u16 wd, u16 ht, u32 fmt, u32 mipmap);

extern u32 gc_gx_cp_tex;
extern u32 gc_gx_cp_tex_stride;
extern u32 gc_gx_cp_tex_z;

const char *gc_scenario_label(void) { return "GXSetTexCopyDst/mp4_shadow_small"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tex_copy_dst_mp4_shadow_small_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // MP4 Hu3DShadowExec small branch (unk_02 <= 0xF0): mipmap=1
    const u16 unk_02 = 0x00F0;
    GXSetTexCopyDst(unk_02, unk_02, GX_CTF_R8, 1);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_cp_tex);
    wr32be(p + 0x08, gc_gx_cp_tex_stride);
    wr32be(p + 0x0C, gc_gx_cp_tex_z);
    wr32be(p + 0x10, 0);
    wr32be(p + 0x14, 0);
    wr32be(p + 0x18, 0);
    wr32be(p + 0x1C, 0);
}

