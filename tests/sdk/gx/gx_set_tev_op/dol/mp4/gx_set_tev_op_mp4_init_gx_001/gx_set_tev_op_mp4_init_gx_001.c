#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

typedef enum { GX_FALSE = 0, GX_TRUE = 1 } GXBool;

typedef enum { GX_TEVSTAGE0 = 0 } GXTevStageID;

typedef enum { GX_MODULATE=0, GX_DECAL=1, GX_BLEND=2, GX_REPLACE=3, GX_PASSCLR=4 } GXTevMode;

typedef enum {
    GX_CC_ZERO=0,
    GX_CC_ONE=1,
    GX_CC_TEXC=8,
    GX_CC_TEXA=9,
    GX_CC_RASC=10,
    GX_CC_CPREV=12,
} GXTevColorArg;

typedef enum {
    GX_CA_ZERO=0,
    GX_CA_TEXA=4,
    GX_CA_RASA=5,
    GX_CA_APREV=6,
} GXTevAlphaArg;

typedef enum { GX_TEV_ADD = 0 } GXTevOp;

typedef enum { GX_TB_ZERO = 0 } GXTevBias;

typedef enum { GX_CS_SCALE_1 = 0 } GXTevScale;

typedef enum { GX_TEVPREV = 0 } GXTevRegID;

static u32 s_tevc[16];
static u32 s_teva[16];
static u32 s_last_ras_reg;
static u32 s_bp_sent_not;

static inline u32 set_field(u32 reg, u32 size, u32 shift, u32 v) {
    const u32 mask = ((1u << size) - 1u) << shift;
    return (reg & ~mask) | ((v << shift) & mask);
}

static void GX_WRITE_RAS_REG(u32 v) {
    s_last_ras_reg = v;
}

static void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d) {
    u32 reg = s_tevc[stage];
    reg = set_field(reg, 4, 12, (u32)a);
    reg = set_field(reg, 4, 8, (u32)b);
    reg = set_field(reg, 4, 4, (u32)c);
    reg = set_field(reg, 4, 0, (u32)d);
    s_tevc[stage] = reg;
    GX_WRITE_RAS_REG(reg);
    s_bp_sent_not = 0;
}

static void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d) {
    u32 reg = s_teva[stage];
    reg = set_field(reg, 3, 13, (u32)a);
    reg = set_field(reg, 3, 10, (u32)b);
    reg = set_field(reg, 3, 7, (u32)c);
    reg = set_field(reg, 3, 4, (u32)d);
    s_teva[stage] = reg;
    GX_WRITE_RAS_REG(reg);
    s_bp_sent_not = 0;
}

static void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) {
    u32 reg = s_tevc[stage];
    reg = set_field(reg, 1, 18, (u32)op & 1u);
    reg = set_field(reg, 2, 20, (u32)scale);
    reg = set_field(reg, 2, 16, (u32)bias);
    reg = set_field(reg, 1, 19, (u32)clamp);
    reg = set_field(reg, 2, 22, (u32)out_reg);
    s_tevc[stage] = reg;
    GX_WRITE_RAS_REG(reg);
    s_bp_sent_not = 0;
}

static void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) {
    u32 reg = s_teva[stage];
    reg = set_field(reg, 1, 18, (u32)op & 1u);
    reg = set_field(reg, 2, 20, (u32)scale);
    reg = set_field(reg, 2, 16, (u32)bias);
    reg = set_field(reg, 1, 19, (u32)clamp);
    reg = set_field(reg, 2, 22, (u32)out_reg);
    s_teva[stage] = reg;
    GX_WRITE_RAS_REG(reg);
    s_bp_sent_not = 0;
}

static void GXSetTevOp(GXTevStageID id, GXTevMode mode) {
    GXTevColorArg carg = GX_CC_RASC;
    GXTevAlphaArg aarg = GX_CA_RASA;

    if (id != GX_TEVSTAGE0) {
        carg = GX_CC_CPREV;
        aarg = GX_CA_APREV;
    }

    switch (mode) {
        case GX_MODULATE:
            GXSetTevColorIn(id, GX_CC_ZERO, GX_CC_TEXC, carg, GX_CC_ZERO);
            GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO);
            break;
        case GX_DECAL:
            GXSetTevColorIn(id, carg, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
            GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg);
            break;
        case GX_BLEND:
            GXSetTevColorIn(id, carg, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
            GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO);
            break;
        case GX_REPLACE:
            GXSetTevColorIn(id, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
            GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
            break;
        case GX_PASSCLR:
            GXSetTevColorIn(id, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, carg);
            GXSetTevAlphaIn(id, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg);
            break;
    }

    GXSetTevColorOp(id, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(id, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

int main(void) {
    u32 i;
    for (i = 0; i < 16; i++) { s_tevc[i] = 0; s_teva[i] = 0; }
    s_last_ras_reg = 0;
    s_bp_sent_not = 1;

    GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = s_tevc[0];
    *(volatile u32 *)0x80300008 = s_teva[0];
    *(volatile u32 *)0x8030000C = s_last_ras_reg;
    *(volatile u32 *)0x80300010 = s_bp_sent_not;
    while (1) {}
}
