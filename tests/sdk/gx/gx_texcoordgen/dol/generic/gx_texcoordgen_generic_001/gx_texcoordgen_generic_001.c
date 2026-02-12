#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t  u8;

/* gc_mem init (needed for sdk_state RAM-backed store). */
#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

/* Functions under test. */
void GXSetTexCoordGen2(u8 dst_coord, u32 func, u32 src_param, u32 mtx,
                       u32 normalize, u32 postmtx);
void GXSetTexCoordGen(u8 dst_coord, u32 func, u32 src_param, u32 mtx);
void GXSetNumTexGens(u8 nTexGens);

/* Observable state from GX.c. */
extern u32 gc_gx_xf_texcoordgen_40[8];
extern u32 gc_gx_xf_texcoordgen_50[8];
extern u32 gc_gx_mat_idx_a;
extern u32 gc_gx_mat_idx_b;
extern u32 gc_gx_xf_regs[64];
extern u32 gc_gx_gen_mode;
extern u32 gc_gx_dirty_state;
extern u32 gc_gx_bp_sent_not;

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

/* GXTexGenType:  MTX3x4=0, MTX2x4=1, SRTG=10 */
/* GXTexGenSrc:   POS=0, NRM=1, BINRM=2, TANGENT=3, TEX0..7=4..11, COLOR0=19, COLOR1=20 */
/* GXTexCoordID:  0..7 */
/* GXTexMtx:      TEXMTX0=30, IDENTITY=60 */
/* GXPTTexMtx:    PTTEXMTX0=64, PTIDENTITY=125 */

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x100; i++) out[i] = 0;

    u32 off = 0;

    /* --- T1: GXSetTexCoordGen2 — TEXCOORD0, MTX2x4, POS, TEXMTX0(30), no norm, PTIDENTITY(125) --- */
    GXSetTexCoordGen2(0, 1, 0, 30, 0, 125);
    wr32be(out + off, 0xCE6E0001u); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_40[0]); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_50[0]); off += 4;
    wr32be(out + off, gc_gx_mat_idx_a); off += 4;
    wr32be(out + off, gc_gx_xf_regs[24]); off += 4;

    /* --- T2: GXSetTexCoordGen2 — TEXCOORD3, MTX3x4, NRM, TEXMTX2(36), normalize=1, PTTEXMTX0(64) --- */
    GXSetTexCoordGen2(3, 0, 1, 36, 1, 64);
    wr32be(out + off, 0xCE6E0002u); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_40[3]); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_50[3]); off += 4;
    wr32be(out + off, gc_gx_mat_idx_a); off += 4;
    wr32be(out + off, gc_gx_xf_regs[24]); off += 4;

    /* --- T3: GXSetTexCoordGen2 — TEXCOORD5, MTX2x4, TEX3(7), TEXMTX5(45), no norm, PTIDENTITY(125) --- */
    GXSetTexCoordGen2(5, 1, 7, 45, 0, 125);
    wr32be(out + off, 0xCE6E0003u); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_40[5]); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_50[5]); off += 4;
    wr32be(out + off, gc_gx_mat_idx_b); off += 4;
    wr32be(out + off, gc_gx_xf_regs[25]); off += 4;

    /* --- T4: GXSetTexCoordGen2 — TEXCOORD7, MTX2x4, COLOR0(19), IDENTITY(60), no norm, PTIDENTITY(125) --- */
    GXSetTexCoordGen2(7, 1, 19, 60, 0, 125);
    wr32be(out + off, 0xCE6E0004u); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_40[7]); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_50[7]); off += 4;
    wr32be(out + off, gc_gx_mat_idx_b); off += 4;
    wr32be(out + off, gc_gx_xf_regs[25]); off += 4;

    /* --- T5: GXSetTexCoordGen (wrapper) — TEXCOORD1, MTX2x4, TEX0(4), TEXMTX1(33) --- */
    GXSetTexCoordGen(1, 1, 4, 33);
    wr32be(out + off, 0xCE6E0005u); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_40[1]); off += 4;
    wr32be(out + off, gc_gx_xf_texcoordgen_50[1]); off += 4;
    wr32be(out + off, gc_gx_mat_idx_a); off += 4;
    wr32be(out + off, gc_gx_xf_regs[24]); off += 4;

    /* --- T6: GXSetNumTexGens — 4 tex gens --- */
    GXSetNumTexGens(4);
    wr32be(out + off, 0xCE6E0006u); off += 4;
    wr32be(out + off, gc_gx_gen_mode); off += 4;
    wr32be(out + off, gc_gx_dirty_state); off += 4;

    /* --- T7: Full state dump of all 8 XF texcoordgen regs --- */
    wr32be(out + off, 0xCE6E0007u); off += 4;
    for (u32 i = 0; i < 8; i++) { wr32be(out + off, gc_gx_xf_texcoordgen_40[i]); off += 4; }
    for (u32 i = 0; i < 8; i++) { wr32be(out + off, gc_gx_xf_texcoordgen_50[i]); off += 4; }
    wr32be(out + off, gc_gx_mat_idx_a); off += 4;
    wr32be(out + off, gc_gx_mat_idx_b); off += 4;
    wr32be(out + off, gc_gx_bp_sent_not); off += 4;

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
