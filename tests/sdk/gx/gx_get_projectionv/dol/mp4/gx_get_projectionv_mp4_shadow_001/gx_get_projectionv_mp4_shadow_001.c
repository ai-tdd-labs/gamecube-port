#include <stdint.h>

typedef float f32;
typedef uint32_t u32;

typedef enum {
    GX_PERSPECTIVE = 0,
    GX_ORTHOGRAPHIC = 1,
} GXProjectionType;

static u32 s_proj_type;
static u32 s_proj_mtx_bits[6];
static u32 s_bp_sent_not;

static void GXSetProjection(f32 mtx[4][4], GXProjectionType type) {
    f32 p0 = mtx[0][0];
    f32 p2 = mtx[1][1];
    f32 p4 = mtx[2][2];
    f32 p5 = mtx[2][3];
    f32 p1;
    f32 p3;

    s_proj_type = (u32)type;
    if (type == GX_ORTHOGRAPHIC) {
        p1 = mtx[0][3];
        p3 = mtx[1][3];
    } else {
        p1 = mtx[0][2];
        p3 = mtx[1][2];
    }

    __builtin_memcpy(&s_proj_mtx_bits[0], &p0, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[1], &p1, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[2], &p2, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[3], &p3, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[4], &p4, sizeof(u32));
    __builtin_memcpy(&s_proj_mtx_bits[5], &p5, sizeof(u32));
    s_bp_sent_not = 1;
}

static void GXGetProjectionv(f32 *ptr) {
    ptr[0] = (f32)s_proj_type;
    __builtin_memcpy(&ptr[1], &s_proj_mtx_bits[0], sizeof(u32));
    __builtin_memcpy(&ptr[2], &s_proj_mtx_bits[1], sizeof(u32));
    __builtin_memcpy(&ptr[3], &s_proj_mtx_bits[2], sizeof(u32));
    __builtin_memcpy(&ptr[4], &s_proj_mtx_bits[3], sizeof(u32));
    __builtin_memcpy(&ptr[5], &s_proj_mtx_bits[4], sizeof(u32));
    __builtin_memcpy(&ptr[6], &s_proj_mtx_bits[5], sizeof(u32));
}

int main(void) {
    f32 mtx[4][4] = {
        {2.0f, 0.0f, 0.0f, 5.0f},
        {0.0f, 3.0f, 0.0f, 7.0f},
        {0.0f, 0.0f, 11.0f, 13.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    f32 out[7];
    u32 *out_bits = (u32 *)out;

    s_proj_type = 0;
    s_proj_mtx_bits[0] = 0;
    s_proj_mtx_bits[1] = 0;
    s_proj_mtx_bits[2] = 0;
    s_proj_mtx_bits[3] = 0;
    s_proj_mtx_bits[4] = 0;
    s_proj_mtx_bits[5] = 0;
    s_bp_sent_not = 0;

    GXSetProjection(mtx, GX_ORTHOGRAPHIC);
    GXGetProjectionv(out);

    *(volatile u32 *)0x80300000 = 0xDEADBEEFu;
    *(volatile u32 *)0x80300004 = out_bits[0];
    *(volatile u32 *)0x80300008 = out_bits[1];
    *(volatile u32 *)0x8030000C = out_bits[2];
    *(volatile u32 *)0x80300010 = out_bits[3];
    *(volatile u32 *)0x80300014 = out_bits[4];
    *(volatile u32 *)0x80300018 = out_bits[5];
    *(volatile u32 *)0x8030001C = out_bits[6];
    *(volatile u32 *)0x80300020 = s_bp_sent_not;
    while (1) {}
}
