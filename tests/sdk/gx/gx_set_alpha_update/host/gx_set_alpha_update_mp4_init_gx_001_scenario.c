#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint8_t u8;

typedef struct { u8 r, g, b, a; } GXColor;

/* Functions under test (sdk_port). */
void GXSetChanCtrl(u32 chan, u8 enable, u32 amb_src, u32 mat_src, u32 light_mask, u32 diff_fn, u32 attn_fn);
void GXSetChanAmbColor(u32 chan, GXColor amb_color);
void GXSetChanMatColor(u32 chan, GXColor mat_color);
void GXSetNumTevStages(u8 nStages);
void GXSetTevOp(u32 id, u32 mode);
void GXSetAlphaCompare(u32 comp0, u8 ref0, u32 op, u32 comp1, u8 ref1);
void GXSetBlendMode(u32 type, u32 src_factor, u32 dst_factor, u32 op);
void GXSetAlphaUpdate(u8 update_enable);
void GXSetZCompLoc(u8 before_tex);
void GXSetDither(u8 dither);

/* State mirrors asserted by the deterministic tests. */
extern u32 gc_gx_bp_sent_not;
extern u32 gc_gx_last_ras_reg;
extern u32 gc_gx_gen_mode;
extern u32 gc_gx_dirty_state;
extern u32 gc_gx_tevc[16];
extern u32 gc_gx_teva[16];
extern u32 gc_gx_xf_regs[32];
extern u32 gc_gx_amb_color[2];
extern u32 gc_gx_mat_color[2];
extern u32 gc_gx_cmode0;
extern u32 gc_gx_pe_ctrl;

const char *gc_scenario_label(void) { return "gx_set_alpha_update/mp4_init_gx"; }
const char *gc_scenario_out_path(void) { return "../actual/gx_set_alpha_update_mp4_init_gx_001.bin"; }

static void zero_state(void) {
    /* Match the DOL tests: start from zeroed local state. */
    gc_gx_bp_sent_not = 0;
    gc_gx_last_ras_reg = 0;
    gc_gx_gen_mode = 0;
    gc_gx_dirty_state = 0;
    gc_gx_cmode0 = 0;
    gc_gx_pe_ctrl = 0;
    for (u32 i = 0; i < 16; i++) {
        gc_gx_tevc[i] = 0;
        gc_gx_teva[i] = 0;
    }
    for (u32 i = 0; i < 32; i++) {
        gc_gx_xf_regs[i] = 0;
    }
    gc_gx_amb_color[0] = 0;
    gc_gx_amb_color[1] = 0;
    gc_gx_mat_color[0] = 0;
    gc_gx_mat_color[1] = 0;
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;
    zero_state();

    /* Callsite-style invocations from MP4 GXInit path. */
    GXSetAlphaUpdate(1u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_gx_cmode0);
    wr32be(p + 0x08, gc_gx_last_ras_reg);
    wr32be(p + 0x0C, gc_gx_bp_sent_not);
}
