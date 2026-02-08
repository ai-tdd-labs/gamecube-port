#include <dolphin/types.h>
#include <dolphin/mtx.h>

// Ground-truth implementation for expected.bin.
// Source: decomp_mario_party_4/src/dolphin/mtx/mtx44.c:C_MTXOrtho
void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f) {
    f32 tmp = 1.0f / (r - l);
    m[0][0] = 2.0f * tmp;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = -(r + l) * tmp;

    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = 2.0f * tmp;
    m[1][2] = 0.0f;
    m[1][3] = -(t + b) * tmp;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(1.0f) * tmp;
    m[2][3] = -(f) * tmp;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

static u32 f2u(f32 f) {
    union { f32 f; u32 u; } x;
    x.f = f;
    return x.u;
}

int main(void) {
    Mtx44 m;
    // t=0,b=1,l=0,r=1,n=0,f=1
    C_MTXOrtho(m, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    // Dump 16 floats as u32 bit patterns.
    *(volatile u32*)0x80300004 = f2u(m[0][0]);
    *(volatile u32*)0x80300008 = f2u(m[0][1]);
    *(volatile u32*)0x8030000C = f2u(m[0][2]);
    *(volatile u32*)0x80300010 = f2u(m[0][3]);
    *(volatile u32*)0x80300014 = f2u(m[1][0]);
    *(volatile u32*)0x80300018 = f2u(m[1][1]);
    *(volatile u32*)0x8030001C = f2u(m[1][2]);
    *(volatile u32*)0x80300020 = f2u(m[1][3]);
    *(volatile u32*)0x80300024 = f2u(m[2][0]);
    *(volatile u32*)0x80300028 = f2u(m[2][1]);
    *(volatile u32*)0x8030002C = f2u(m[2][2]);
    *(volatile u32*)0x80300030 = f2u(m[2][3]);
    *(volatile u32*)0x80300034 = f2u(m[3][0]);
    *(volatile u32*)0x80300038 = f2u(m[3][1]);
    *(volatile u32*)0x8030003C = f2u(m[3][2]);
    // m[3][3] doesn't fit in 0x40 window; keep it implicit (known 1.0f).

    while (1) {}
}

