#include <dolphin/types.h>
#include <dolphin/mtx.h>

// Ground-truth implementation for expected.bin.
// Source: decomp_mario_party_4/src/dolphin/mtx/mtx.c:C_MTXIdentity
void C_MTXIdentity(Mtx mtx) {
    mtx[0][0] = 1.0f;
    mtx[0][1] = 0.0f;
    mtx[0][2] = 0.0f;
    mtx[0][3] = 0.0f;

    mtx[1][0] = 0.0f;
    mtx[1][1] = 1.0f;
    mtx[1][2] = 0.0f;
    mtx[1][3] = 0.0f;

    mtx[2][0] = 0.0f;
    mtx[2][1] = 0.0f;
    mtx[2][2] = 1.0f;
    mtx[2][3] = 0.0f;
}

static u32 f2u(f32 f) {
    union { f32 f; u32 u; } x;
    x.f = f;
    return x.u;
}

int main(void) {
    Mtx m;
    // Fill with noise to prove we overwrite deterministically.
    int r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            m[r][c] = -123.0f;
        }
    }

    C_MTXIdentity(m);

    *(volatile u32*)0x80300000 = 0xDEADBEEF;
    // Dump 12 floats (3x4) as u32 bit patterns.
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

    while (1) {}
}
