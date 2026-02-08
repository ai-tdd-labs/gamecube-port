#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;

enum { GX_CTF_R8 = 0x4 };

void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht);
void GXSetTexCopyDst(u16 wd, u16 ht, u32 fmt, u32 mipmap);
void GXCopyTex(void *dest, u32 clear);
void GXSetZMode(uint8_t enable, u32 func, uint8_t update_enable);

extern u32 gc_gx_cp_tex_src;
extern u32 gc_gx_cp_tex_size;
extern u32 gc_gx_cp_tex_stride;
extern u32 gc_gx_cp_tex_addr_reg;
extern u32 gc_gx_cp_tex_written_reg;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_cmode0;
extern u32 gc_gx_zmode;

const char *gc_scenario_label(void) { return "GXCopyTex/mp4_shadow_small"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_copy_tex_mp4_shadow_small_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // MP4 Hu3DShadowExec small branch (unk_02 <= 0xF0): copy region is 0..unk_02*2.
    const u16 unk_02 = 0x00F0;
    GXSetTexCopySrc(0, 0, (u16)(unk_02 * 2), (u16)(unk_02 * 2));
    GXSetTexCopyDst(unk_02, unk_02, GX_CTF_R8, 1);

    // Ensure clear-path writes are deterministic (GXCopyTex(clear=1) writes zmode+cmode0).
    GXSetZMode(1, 7, 1);
    gc_gx_cmode0 = 0; // baseline so clear-path cmode reg is stable

    // The real dest is a MEM1 heap ptr. For deterministic packing, only low 30 bits matter.
    void *dest = (void *)0x81234560u;
    GXCopyTex(dest, 1);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_cp_tex_src);
    wr32be(p + 0x08, gc_gx_cp_tex_size);
    wr32be(p + 0x0C, gc_gx_cp_tex_stride);
    wr32be(p + 0x10, gc_gx_cp_tex_addr_reg);
    wr32be(p + 0x14, gc_gx_cp_tex_written_reg);
    wr32be(p + 0x18, gc_gx_zmode);
    // The clear path writes a temporary cmode register (reg-id 0x42) to the
    // RAS stream; gc_gx_cmode0 is internal state and does not include the id.
    wr32be(p + 0x1C, gc_gx_last_ras_reg);
    wr32be(p + 0x20, 0);
    wr32be(p + 0x24, 0);
    wr32be(p + 0x28, 0);
    wr32be(p + 0x2C, 0);
    wr32be(p + 0x30, 0);
    wr32be(p + 0x34, 0);
    wr32be(p + 0x38, 0);
    wr32be(p + 0x3C, 0);
}
