#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht);

extern u32 gc_gx_cp_tex_src;
extern u32 gc_gx_cp_tex_size;

const char *gc_scenario_label(void) { return "GXSetTexCopySrc/mp4_shadow_small"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_tex_copy_src_mp4_shadow_small_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // MP4 Hu3DShadowExec: left=0 top=0 wd=unk_02*2 ht=unk_02*2 (for unk_02 <= 0xF0)
    const u16 unk_02 = 0x00F0;
    GXSetTexCopySrc(0, 0, (u16)(unk_02 * 2), (u16)(unk_02 * 2));

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_cp_tex_src);
    wr32be(p + 0x08, gc_gx_cp_tex_size);
    wr32be(p + 0x0C, 0);
}

