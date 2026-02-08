#include <stdint.h>

typedef uint32_t u32;

typedef u32 GXAttr;
enum { GX_VA_PNMTXIDX = 0 };

static u32 s_mat_idx_a;
static u32 s_xf_reg24;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_XF_REG(u32 idx, u32 v) {
    if (idx == 24) s_xf_reg24 = v;
}

static void __GXSetMatrixIndex(GXAttr matIdxAttr) {
    (void)matIdxAttr;
    GX_WRITE_XF_REG(24, s_mat_idx_a);
    s_bp_sent_not = 1;
}

static void GXSetCurrentMtx(u32 id) {
    // Mirror decomp_mario_party_4/src/dolphin/gx/GXTransform.c:GXSetCurrentMtx
    s_mat_idx_a = set_field(s_mat_idx_a, 6, 0, id);
    __GXSetMatrixIndex(GX_VA_PNMTXIDX);
}

int main(void) {
    // MP4 Hu3DExec: GXSetCurrentMtx(0)
    s_mat_idx_a = 0;
    s_xf_reg24 = 0;
    s_bp_sent_not = 0;

    GXSetCurrentMtx(0u);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = s_mat_idx_a;
    *(volatile u32*)0x80300008 = s_xf_reg24;
    *(volatile u32*)0x8030000C = s_bp_sent_not;
    while (1) {}
}
