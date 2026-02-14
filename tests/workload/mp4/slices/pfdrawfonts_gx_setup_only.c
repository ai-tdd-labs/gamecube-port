// Keep this slice self-contained for host workload builds:
// - The workload uses a minimal `tests/workload/include/dolphin/gx.h` surface.
// - When we need extra GX pieces (types/constants/prototypes) we define only
//   values that are already proven elsewhere in this repo (GX oracle suites).
#include "dolphin/gx.h"
#include <stdint.h>

// MP4 host workload slice: pfDrawFonts() GX setup only.
//
// Goal:
// - Exercise the same GX state setup that MP4's real pfDrawFonts() performs,
//   without pulling in MTX/projection code or the full text rendering loop.
// - Keep workloads reachability-focused (no correctness oracle).
//
// Source reference:
// - decomp_mario_party_4/src/game/printfunc.c:pfDrawFonts

// RenderMode is initialized by HuSysInit() in src/game_workload/mp4/vendor/src/game/init.c.
extern GXRenderModeObj *RenderMode;

// Additional GX pieces not present in the workload's minimal header.
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef float f32;

// GXTexObj layout as used by our existing GX texture oracle suites.
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

void GXInitTexObj(GXTexObj *obj, void *image_ptr, u16 width, u16 height, u32 format, u32 wrap_s, u32 wrap_t, u8 mipmap);
void GXInitTexObjLOD(GXTexObj *obj, u32 min_filt, u32 mag_filt, f32 min_lod, f32 max_lod, f32 lod_bias, u8 bias_clamp,
                     u8 do_edge_lod, u32 max_aniso);
void GXLoadTexObj(GXTexObj *obj, u32 id);
void GXSetTexCoordGen(u8 dst_coord, u8 func, u8 src_param, u32 mtx);

// Constants: use values that are already embedded in our deterministic GX tests.
enum {
    GX_ALWAYS = 7, // tests/sdk/gx/gx_set_alpha_compare/* defines GX_ALWAYS=7

    GX_DIRECT = 1, // tests/sdk/gx/gx_set_vtx_desc/* defines GX_DIRECT=1
    GX_VA_POS = 9, // src/sdk_port/gx/GX.c enum extracted from MP4 decomp

    GX_POS_XYZ = 1, // tests/sdk/gx/gx_set_vtx_attr_fmt/* defines GX_POS_XYZ=1
    GX_F32 = 4,     // tests/sdk/gx/gx_set_vtx_attr_fmt/* defines GX_F32=4

    GX_TF_I4 = 0x0, // tests/sdk/gx/property/gxtexture_property_test.c
    GX_CLAMP = 0,   // tests/sdk/gx/gx_load_tex_obj/* defines GX_CLAMP=0
    GX_NEAR = 0,    // tests/sdk/gx/gx_init_tex_obj_lod/* defines GX_NEAR=0
    GX_ANISO_1 = 0, // tests/sdk/gx/gx_init_tex_obj_lod/* defines GX_ANISO_1=0

    GX_TEXMAP0 = 0,   // tests/sdk/gx/gx_set_tev_order/* defines GX_TEXMAP0=0
    GX_TEVSTAGE0 = 0, // tests/sdk/gx/gx_set_tev_order/* defines GX_TEVSTAGE0=0
    GX_TEXCOORD0 = 0, // tests/sdk/gx/gx_set_tev_order/* defines GX_TEXCOORD0=0
    GX_COLOR0A0 = 4,  // tests/sdk/gx/gx_set_tev_order/* defines GX_COLOR0A0=4

    GX_REPLACE = 3, // tests/sdk/gx/gx_set_tev_op/* defines GX_REPLACE=3

    GX_TG_MTX2x4 = 1, // tests/sdk/gx/gx_set_tex_coord_gen/*
    GX_TG_TEX0 = 4,   // tests/sdk/gx/gx_set_tex_coord_gen/*
    GX_IDENTITY = 60, // tests/sdk/gx/gx_set_tex_coord_gen/*

    GX_SRC_REG = 0, // tests/sdk/gx/gx_set_chan_ctrl/*
    GX_SRC_VTX = 1, // tests/sdk/gx/gx_set_chan_ctrl/*
    GX_DF_NONE = 0, // tests/sdk/gx/gx_set_chan_ctrl/*
    GX_AF_NONE = 0, // tests/sdk/gx/gx_set_chan_ctrl/*
};

// Minimal font texture payload to satisfy GXInitTexObj source pointer.
static uint8_t s_font_tex_i4[128u * 128u / 2u];

void pfDrawFonts(void)
{
    if (!RenderMode) {
        return;
    }

    // Mirror the state setup in MP4 pfDrawFonts(), but skip MTXOrtho/Projection/Mtx loads.
    GXSetViewport(0, 0, (float)RenderMode->fbWidth, (float)RenderMode->efbHeight, 0.0f, 1.0f);
    GXSetScissor(0, 0, RenderMode->fbWidth, RenderMode->efbHeight);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxAttrFmt(0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXInvalidateTexAll();

    // Exercise the GX texture path (still reachability-only).
    GXTexObj font_tex;
    GXInitTexObj(&font_tex, s_font_tex_i4, 128, 128, GX_TF_I4, GX_CLAMP, GX_CLAMP, 0);
    GXInitTexObjLOD(&font_tex, GX_NEAR, GX_NEAR, 0.0f, 0.0f, 0.0f, 0, 0, GX_ANISO_1);
    GXLoadTexObj(&font_tex, GX_TEXMAP0);

    GXSetNumTevStages(1);
    GXSetNumTexGens(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTexCoordGen((u8)GX_TEXCOORD0, (u8)GX_TG_MTX2x4, (u8)GX_TG_TEX0, (u32)GX_IDENTITY);
    GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);

    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
    GXSetAlphaUpdate(GX_TRUE);
}
