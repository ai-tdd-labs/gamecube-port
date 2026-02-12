/*
 * mtx44_oracle.h --- Oracle for MTX44 projection matrix property tests.
 *
 * Exact copy of decomp C_MTXFrustum, C_MTXPerspective, C_MTXOrtho
 * with oracle_ prefix.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/mtx/mtx44.c
 */
#pragma once

#include <math.h>

typedef float f32;

typedef f32 oracle_Mtx44[4][4];

#define ORACLE_MTXDegToRad(a) ((a) * 0.017453292f)

/* ── C_MTXFrustum (mtx44.c:4-25) ── */
static void oracle_C_MTXFrustum(oracle_Mtx44 m,
    f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6)
{
    f32 tmp = 1.0f / (arg4 - arg3);
    m[0][0] = (2 * arg5) * tmp;
    m[0][1] = 0.0f;
    m[0][2] = (arg4 + arg3) * tmp;
    m[0][3] = 0.0f;
    tmp = 1.0f / (arg1 - arg2);
    m[1][0] = 0.0f;
    m[1][1] = (2 * arg5) * tmp;
    m[1][2] = (arg1 + arg2) * tmp;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    tmp = 1.0f / (arg6 - arg5);
    m[2][2] = -(arg5) * tmp;
    m[2][3] = -(arg6 * arg5) * tmp;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = -1.0f;
    m[3][3] = 0.0f;
}

/* ── C_MTXPerspective (mtx44.c:28-52) ── */
static void oracle_C_MTXPerspective(oracle_Mtx44 m,
    f32 fovY, f32 aspect, f32 n, f32 f)
{
    f32 angle = fovY * 0.5f;
    f32 cot, tmp;
    angle = ORACLE_MTXDegToRad(angle);
    cot = 1.0f / tanf(angle);
    m[0][0] = cot / aspect;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = cot;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(n) * tmp;
    m[2][3] = -(f * n) * tmp;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = -1.0f;
    m[3][3] = 0.0f;
}

/* ── C_MTXOrtho (mtx44.c:54-75) ── */
static void oracle_C_MTXOrtho(oracle_Mtx44 m,
    f32 t, f32 b, f32 l, f32 r, f32 n, f32 f)
{
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
