/*
 * gxlight_oracle.h --- Oracle for GXLight property tests.
 *
 * Exact copy of decomp GXInitLightSpot, GXInitLightDistAttn,
 * GXInitSpecularDir, GXInitLightAttn, GXInitLightAttnK, GXInitLightColor
 * with oracle_ prefix.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/gx/GXLight.c
 */
#pragma once

#include <stdint.h>
#include <math.h>

typedef float f32;
typedef unsigned int u32;
typedef unsigned char u8;

typedef struct {
    u32 reserved[3];
    u32 Color;
    f32 a[3];
    f32 k[3];
    f32 lpos[3];
    f32 ldir[3];
} oracle_GXLightObj;

typedef struct { u8 r, g, b, a; } oracle_GXColor;

typedef enum {
    ORACLE_GX_SP_OFF,
    ORACLE_GX_SP_FLAT,
    ORACLE_GX_SP_COS,
    ORACLE_GX_SP_COS2,
    ORACLE_GX_SP_SHARP,
    ORACLE_GX_SP_RING1,
    ORACLE_GX_SP_RING2,
} oracle_GXSpotFn;

typedef enum {
    ORACLE_GX_DA_OFF,
    ORACLE_GX_DA_GENTLE,
    ORACLE_GX_DA_MEDIUM,
    ORACLE_GX_DA_STEEP,
} oracle_GXDistAttnFn;

/* ── GXInitLightAttn (GXLight.c:34-47) ── */
static void oracle_GXInitLightAttn(oracle_GXLightObj *obj,
    f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2)
{
    obj->a[0] = a0; obj->a[1] = a1; obj->a[2] = a2;
    obj->k[0] = k0; obj->k[1] = k1; obj->k[2] = k2;
}

/* ── GXInitLightAttnK (GXLight.c:73-83) ── */
static void oracle_GXInitLightAttnK(oracle_GXLightObj *obj, f32 k0, f32 k1, f32 k2)
{
    obj->k[0] = k0; obj->k[1] = k1; obj->k[2] = k2;
}

/* ── GXInitLightSpot (GXLight.c:97-156) ── */
static void oracle_GXInitLightSpot(oracle_GXLightObj *obj,
    f32 cutoff, oracle_GXSpotFn spot_func)
{
    f32 a0, a1, a2;
    f32 d, cr;

    if (cutoff <= 0.0f || cutoff > 90.0f)
        spot_func = ORACLE_GX_SP_OFF;

    cr = cosf((3.1415927f * cutoff) / 180.0f);
    switch (spot_func) {
        case ORACLE_GX_SP_FLAT:
            a0 = -1000.0f * cr;
            a1 = 1000.0f;
            a2 = 0.0f;
            break;
        case ORACLE_GX_SP_COS:
            a0 = -cr / (1.0f - cr);
            a1 = 1.0f / (1.0f - cr);
            a2 = 0.0f;
            break;
        case ORACLE_GX_SP_COS2:
            a0 = 0.0f;
            a1 = -cr / (1.0f - cr);
            a2 = 1.0f / (1.0f - cr);
            break;
        case ORACLE_GX_SP_SHARP:
            d = (1.0f - cr) * (1.0f - cr);
            a0 = (cr * (cr - 2.0f)) / d;
            a1 = 2.0f / d;
            a2 = -1.0f / d;
            break;
        case ORACLE_GX_SP_RING1:
            d = (1.0f - cr) * (1.0f - cr);
            a0 = (-4.0f * cr) / d;
            a1 = (4.0f * (1.0f + cr)) / d;
            a2 = -4.0f / d;
            break;
        case ORACLE_GX_SP_RING2:
            d = (1.0f - cr) * (1.0f - cr);
            a0 = 1.0f - ((2.0f * cr * cr) / d);
            a1 = (4.0f * cr) / d;
            a2 = -2.0f / d;
            break;
        case ORACLE_GX_SP_OFF:
        default:
            a0 = 1.0f; a1 = 0.0f; a2 = 0.0f;
            break;
    }
    obj->a[0] = a0; obj->a[1] = a1; obj->a[2] = a2;
}

/* ── GXInitLightDistAttn (GXLight.c:158-199) ── */
static void oracle_GXInitLightDistAttn(oracle_GXLightObj *obj,
    f32 ref_dist, f32 ref_br, oracle_GXDistAttnFn dist_func)
{
    f32 k0, k1, k2;

    if (ref_dist < 0.0f) dist_func = ORACLE_GX_DA_OFF;
    if (ref_br <= 0.0f || ref_br >= 1.0f) dist_func = ORACLE_GX_DA_OFF;

    switch (dist_func) {
        case ORACLE_GX_DA_GENTLE:
            k0 = 1.0f;
            k1 = (1.0f - ref_br) / (ref_br * ref_dist);
            k2 = 0.0f;
            break;
        case ORACLE_GX_DA_MEDIUM:
            k0 = 1.0f;
            k1 = (0.5f * (1.0f - ref_br)) / (ref_br * ref_dist);
            k2 = (0.5f * (1.0f - ref_br)) / (ref_br * ref_dist * ref_dist);
            break;
        case ORACLE_GX_DA_STEEP:
            k0 = 1.0f;
            k1 = 0.0f;
            k2 = (1.0f - ref_br) / (ref_br * ref_dist * ref_dist);
            break;
        case ORACLE_GX_DA_OFF:
        default:
            k0 = 1.0f; k1 = 0.0f; k2 = 0.0f;
            break;
    }
    obj->k[0] = k0; obj->k[1] = k1; obj->k[2] = k2;
}

/* ── GXInitSpecularDir (GXLight.c:251-273) ── */
static void oracle_GXInitSpecularDir(oracle_GXLightObj *obj,
    f32 nx, f32 ny, f32 nz)
{
    f32 mag, vx, vy, vz;

    vx = -nx;
    vy = -ny;
    vz = -nz + 1.0f;
    mag = 1.0f / sqrtf((vx * vx) + (vy * vy) + (vz * vz));
    obj->ldir[0] = vx * mag;
    obj->ldir[1] = vy * mag;
    obj->ldir[2] = vz * mag;
    obj->lpos[0] = -nx * 1048576.0f;
    obj->lpos[1] = -ny * 1048576.0f;
    obj->lpos[2] = -nz * 1048576.0f;
}

/* ── GXInitLightColor (GXLight.c:291-299) ── */
static void oracle_GXInitLightColor(oracle_GXLightObj *obj, oracle_GXColor color)
{
    obj->Color = ((u32)color.r << 24) | ((u32)color.g << 16) | ((u32)color.b << 8) | (u32)color.a;
}
