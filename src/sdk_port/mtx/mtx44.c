/*
 * sdk_port/mtx/mtx44.c — Host-side port of GC SDK 4x4 matrix functions.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/mtx/mtx44.c
 * Exact copy of all C_MTX* 4x4 functions from the decomp.
 */
#include <math.h>
#include "sdk_port_mtx_types.h"

void C_MTXFrustum(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f)
{
    f32 tmp = 1.0f / (r - l);
    m[0][0] = (2*n)*tmp; m[0][1] = 0.0f; m[0][2] = (r+l)*tmp; m[0][3] = 0.0f;
    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f; m[1][1] = (2*n)*tmp; m[1][2] = (t+b)*tmp; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(n)*tmp; m[2][3] = -(f*n)*tmp;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = -1.0f; m[3][3] = 0.0f;
}

void C_MTXPerspective(Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f)
{
    f32 angle = fovY * 0.5f;
    f32 cot, tmp;
    angle = MTXDegToRad(angle);
    cot = 1.0f / tanf(angle);

    m[0][0] = cot/aspect; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = cot; m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(n)*tmp; m[2][3] = -(f*n)*tmp;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = -1.0f; m[3][3] = 0.0f;
}

void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f)
{
    f32 tmp = 1.0f / (r - l);
    m[0][0] = 2.0f*tmp; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = -(r+l)*tmp;
    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f; m[1][1] = 2.0f*tmp; m[1][2] = 0.0f; m[1][3] = -(t+b)*tmp;
    m[2][0] = 0.0f; m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(1.0f)*tmp; m[2][3] = -(f)*tmp;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
}
