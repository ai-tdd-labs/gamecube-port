/*
 * mtx_strict_oracle.h
 *
 * Strict decomp-derived leaf oracle for MTX core math.
 * Source of truth: decomp_mario_party_4/src/dolphin/mtx/mtx.c + mtx44.c
 */
#pragma once

typedef float strict_f32;
typedef strict_f32 strict_Mtx[3][4];
typedef strict_f32 strict_Mtx44[4][4];

static void strict_C_MTXIdentity(strict_Mtx mtx) {
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

static void strict_C_MTXOrtho(strict_Mtx44 m, strict_f32 t, strict_f32 b,
                              strict_f32 l, strict_f32 r, strict_f32 n, strict_f32 f) {
    strict_f32 tmp = 1.0f / (r - l);
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
    m[2][2] = -1.0f * tmp;
    m[2][3] = -f * tmp;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}
