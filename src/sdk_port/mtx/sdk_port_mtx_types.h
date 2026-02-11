/*
 * sdk_port_mtx_types.h — Shared types for sdk_port MTX/VEC/QUAT.
 *
 * Matches the GC SDK types from dolphin/mtx/GeoTypes.h and dolphin/types.h.
 */
#pragma once

typedef float           f32;
typedef unsigned int    u32;
typedef int             s32;

typedef f32 Mtx[3][4];
typedef f32 (*MtxPtr)[4];
typedef f32 Mtx44[4][4];

typedef struct { f32 x, y, z; }       Vec;
typedef struct { f32 x, y, z, w; }    Quaternion;

/* Aliases used in decomp */
typedef Quaternion Qtrn;
typedef Vec Point3d;

#define MTXDegToRad(a) ((a) * 0.017453292f)

/* Forward declarations for cross-file calls (mtx.c uses vec.c functions) */
void C_VECAdd(const Vec *a, const Vec *b, Vec *ab);
void C_VECSubtract(const Vec *a, const Vec *b, Vec *a_b);
f32  C_VECDotProduct(const Vec *a, const Vec *b);
void C_VECCrossProduct(const Vec *a, const Vec *b, Vec *axb);
void C_VECNormalize(const Vec *src, Vec *unit);
