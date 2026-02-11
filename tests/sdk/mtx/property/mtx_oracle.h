/*
 * mtx_oracle.h — Oracle implementation of GC SDK MTX/VEC/QUAT for property testing.
 *
 * EXACT copy of the MP4 decomp's C_* functions from:
 *   external/mp4-decomp/src/dolphin/mtx/mtx.c
 *   external/mp4-decomp/src/dolphin/mtx/vec.c
 *   external/mp4-decomp/src/dolphin/mtx/quat.c
 *   external/mp4-decomp/src/dolphin/mtx/mtx44.c
 *   external/mp4-decomp/src/dolphin/mtx/mtxvec.c
 *
 * Changes from decomp:
 *   1. All symbols get oracle_ prefix
 *   2. Self-contained types (no dolphin headers needed)
 *   3. GEKKO asm blocks removed (C fallbacks only)
 *   4. Leaf VEC/QUAT functions that only had PS* asm in decomp
 *      are given trivial C implementations (basic math)
 *   5. Internal VEC/QUAT macro calls resolve to oracle_ versions
 *
 * No -m32 needed: pure float math, no pointer-size dependencies.
 */
#pragma once

#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Types                                                              */
/* ------------------------------------------------------------------ */

typedef float           oracle_f32;
typedef unsigned int    oracle_u32;
typedef int             oracle_s32;

typedef oracle_f32 oracle_Mtx[3][4];
typedef oracle_f32 (*oracle_MtxPtr)[4];

typedef oracle_f32 oracle_Mtx44[4][4];

typedef struct { oracle_f32 x, y, z; }       oracle_Vec;
typedef struct { oracle_f32 x, y, z, w; }    oracle_Quaternion;

#define oracle_MTXDegToRad(a) ((a) * 0.017453292f)

/* ================================================================== */
/*  VEC — Leaf functions                                               */
/*  (decomp only has PS* asm for most; trivial C implementations)      */
/* ================================================================== */

static void oracle_C_VECAdd(const oracle_Vec *a, const oracle_Vec *b, oracle_Vec *ab)
{
    ab->x = a->x + b->x;
    ab->y = a->y + b->y;
    ab->z = a->z + b->z;
}

static void oracle_C_VECSubtract(const oracle_Vec *a, const oracle_Vec *b, oracle_Vec *a_b)
{
    a_b->x = a->x - b->x;
    a_b->y = a->y - b->y;
    a_b->z = a->z - b->z;
}

static oracle_f32 oracle_C_VECDotProduct(const oracle_Vec *a, const oracle_Vec *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static void oracle_C_VECCrossProduct(const oracle_Vec *a, const oracle_Vec *b, oracle_Vec *axb)
{
    oracle_Vec tmp;
    tmp.x = a->y * b->z - a->z * b->y;
    tmp.y = a->z * b->x - a->x * b->z;
    tmp.z = a->x * b->y - a->y * b->x;
    *axb = tmp;
}

static void oracle_C_VECNormalize(const oracle_Vec *src, oracle_Vec *unit)
{
    oracle_f32 mag = sqrtf(src->x * src->x + src->y * src->y + src->z * src->z);
    if (mag == 0.0f) {
        unit->x = unit->y = unit->z = 0.0f;
        return;
    }
    oracle_f32 inv = 1.0f / mag;
    unit->x = src->x * inv;
    unit->y = src->y * inv;
    unit->z = src->z * inv;
}

static oracle_f32 oracle_C_VECSquareMag(const oracle_Vec *v)
{
    return v->x * v->x + v->y * v->y + v->z * v->z;
}

static oracle_f32 oracle_C_VECMag(const oracle_Vec *v)
{
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

static oracle_f32 oracle_C_VECSquareDistance(const oracle_Vec *a, const oracle_Vec *b)
{
    oracle_f32 dx = a->x - b->x;
    oracle_f32 dy = a->y - b->y;
    oracle_f32 dz = a->z - b->z;
    return dx * dx + dy * dy + dz * dz;
}

static oracle_f32 oracle_C_VECDistance(const oracle_Vec *a, const oracle_Vec *b)
{
    return sqrtf(oracle_C_VECSquareDistance(a, b));
}

/* --- Decomp C_VECScale (vec.c line 64) — NOTE: decomp ignores scale param,
       does normalize. We keep exact decomp logic. --- */
static void oracle_C_VECScale(const oracle_Vec *src, oracle_Vec *dst, oracle_f32 scale)
{
    oracle_f32 s;
    (void)scale; /* decomp ignores this param — it's a normalize */
    s = 1.0f / sqrtf(src->z * src->z + src->x * src->x + src->y * src->y);
    dst->x = src->x * s;
    dst->y = src->y * s;
    dst->z = src->z * s;
}

/* --- Decomp C_VECHalfAngle (vec.c line 185) --- */
static void oracle_C_VECHalfAngle(const oracle_Vec *a, const oracle_Vec *b, oracle_Vec *half)
{
    oracle_Vec a0, b0, ab;

    a0.x = -a->x; a0.y = -a->y; a0.z = -a->z;
    b0.x = -b->x; b0.y = -b->y; b0.z = -b->z;

    oracle_C_VECNormalize(&a0, &a0);
    oracle_C_VECNormalize(&b0, &b0);
    oracle_C_VECAdd(&a0, &b0, &ab);

    if (oracle_C_VECDotProduct(&ab, &ab) > 0.0f) {
        oracle_C_VECNormalize(&ab, half);
    } else {
        *half = ab;
    }
}

/* --- Decomp C_VECReflect (vec.c line 211) --- */
static void oracle_C_VECReflect(const oracle_Vec *src, const oracle_Vec *normal, oracle_Vec *dst)
{
    oracle_Vec a0, b0;
    oracle_f32 dot;

    a0.x = -src->x; a0.y = -src->y; a0.z = -src->z;
    oracle_C_VECNormalize(&a0, &a0);
    oracle_C_VECNormalize(normal, &b0);

    dot = oracle_C_VECDotProduct(&a0, &b0);
    dst->x = b0.x * 2.0f * dot - a0.x;
    dst->y = b0.y * 2.0f * dot - a0.y;
    dst->z = b0.z * 2.0f * dot - a0.z;

    oracle_C_VECNormalize(dst, dst);
}

/* ================================================================== */
/*  MTX — 3x4 matrix functions (decomp mtx.c, exact copy)             */
/* ================================================================== */

static void oracle_C_MTXIdentity(oracle_Mtx mtx)
{
    mtx[0][0] = 1.0f; mtx[0][1] = 0.0f; mtx[0][2] = 0.0f;
    mtx[1][0] = 0.0f; mtx[1][1] = 1.0f; mtx[1][2] = 0.0f;
    mtx[2][0] = 0.0f; mtx[2][1] = 0.0f; mtx[2][2] = 1.0f;
}

static void oracle_C_MTXCopy(const oracle_Mtx src, oracle_Mtx dst)
{
    if (src == dst) return;

    dst[0][0] = src[0][0]; dst[0][1] = src[0][1];
    dst[0][2] = src[0][2]; dst[0][3] = src[0][3];
    dst[1][0] = src[1][0]; dst[1][1] = src[1][1];
    dst[1][2] = src[1][2]; dst[1][3] = src[1][3];
    dst[2][0] = src[2][0]; dst[2][1] = src[2][1];
    dst[2][2] = src[2][2]; dst[2][3] = src[2][3];
}

static void oracle_C_MTXConcat(const oracle_Mtx a, const oracle_Mtx b, oracle_Mtx ab)
{
    oracle_Mtx mTmp;
    oracle_MtxPtr m;

    if ((ab == a) || (ab == b)) {
        m = mTmp;
    } else {
        m = ab;
    }

    m[0][0] = a[0][0]*b[0][0] + a[0][1]*b[1][0] + a[0][2]*b[2][0];
    m[0][1] = a[0][0]*b[0][1] + a[0][1]*b[1][1] + a[0][2]*b[2][1];
    m[0][2] = a[0][0]*b[0][2] + a[0][1]*b[1][2] + a[0][2]*b[2][2];
    m[0][3] = a[0][0]*b[0][3] + a[0][1]*b[1][3] + a[0][2]*b[2][3] + a[0][3];

    m[1][0] = a[1][0]*b[0][0] + a[1][1]*b[1][0] + a[1][2]*b[2][0];
    m[1][1] = a[1][0]*b[0][1] + a[1][1]*b[1][1] + a[1][2]*b[2][1];
    m[1][2] = a[1][0]*b[0][2] + a[1][1]*b[1][2] + a[1][2]*b[2][2];
    m[1][3] = a[1][0]*b[0][3] + a[1][1]*b[1][3] + a[1][2]*b[2][3] + a[1][3];

    m[2][0] = a[2][0]*b[0][0] + a[2][1]*b[1][0] + a[2][2]*b[2][0];
    m[2][1] = a[2][0]*b[0][1] + a[2][1]*b[1][1] + a[2][2]*b[2][1];
    m[2][2] = a[2][0]*b[0][2] + a[2][1]*b[1][2] + a[2][2]*b[2][2];
    m[2][3] = a[2][0]*b[0][3] + a[2][1]*b[1][3] + a[2][2]*b[2][3] + a[2][3];

    if (m == mTmp) {
        oracle_C_MTXCopy(mTmp, ab);
    }
}

static void oracle_C_MTXConcatArray(const oracle_Mtx a, const oracle_Mtx *srcBase,
                                    oracle_Mtx *dstBase, oracle_u32 count)
{
    oracle_u32 i;
    for (i = 0; i < count; i++) {
        oracle_C_MTXConcat(a, *srcBase, *dstBase);
        srcBase++;
        dstBase++;
    }
}

static void oracle_C_MTXTranspose(const oracle_Mtx src, oracle_Mtx xPose)
{
    oracle_Mtx mTmp;
    oracle_MtxPtr m;

    if (src == xPose) { m = mTmp; } else { m = xPose; }

    m[0][0] = src[0][0]; m[0][1] = src[1][0]; m[0][2] = src[2][0]; m[0][3] = 0.0f;
    m[1][0] = src[0][1]; m[1][1] = src[1][1]; m[1][2] = src[2][1]; m[1][3] = 0.0f;
    m[2][0] = src[0][2]; m[2][1] = src[1][2]; m[2][2] = src[2][2]; m[2][3] = 0.0f;

    if (m == mTmp) { oracle_C_MTXCopy(mTmp, xPose); }
}

static oracle_u32 oracle_C_MTXInverse(const oracle_Mtx src, oracle_Mtx inv)
{
    oracle_Mtx mTmp;
    oracle_MtxPtr m;
    oracle_f32 det;

    if (src == inv) { m = mTmp; } else { m = inv; }

    det = src[0][0]*src[1][1]*src[2][2] + src[0][1]*src[1][2]*src[2][0]
        + src[0][2]*src[1][0]*src[2][1] - src[2][0]*src[1][1]*src[0][2]
        - src[1][0]*src[0][1]*src[2][2] - src[0][0]*src[2][1]*src[1][2];

    if (det == 0.0f) return 0;
    det = 1.0f / det;

    m[0][0] =  (src[1][1]*src[2][2] - src[2][1]*src[1][2]) * det;
    m[0][1] = -(src[0][1]*src[2][2] - src[2][1]*src[0][2]) * det;
    m[0][2] =  (src[0][1]*src[1][2] - src[1][1]*src[0][2]) * det;

    m[1][0] = -(src[1][0]*src[2][2] - src[2][0]*src[1][2]) * det;
    m[1][1] =  (src[0][0]*src[2][2] - src[2][0]*src[0][2]) * det;
    m[1][2] = -(src[0][0]*src[1][2] - src[1][0]*src[0][2]) * det;

    m[2][0] =  (src[1][0]*src[2][1] - src[2][0]*src[1][1]) * det;
    m[2][1] = -(src[0][0]*src[2][1] - src[2][0]*src[0][1]) * det;
    m[2][2] =  (src[0][0]*src[1][1] - src[1][0]*src[0][1]) * det;

    m[0][3] = -m[0][0]*src[0][3] - m[0][1]*src[1][3] - m[0][2]*src[2][3];
    m[1][3] = -m[1][0]*src[0][3] - m[1][1]*src[1][3] - m[1][2]*src[2][3];
    m[2][3] = -m[2][0]*src[0][3] - m[2][1]*src[1][3] - m[2][2]*src[2][3];

    if (m == mTmp) { oracle_C_MTXCopy(mTmp, inv); }
    return 1;
}

static oracle_u32 oracle_C_MTXInvXpose(const oracle_Mtx src, oracle_Mtx invX)
{
    oracle_Mtx mTmp;
    oracle_MtxPtr m;
    oracle_f32 det;

    if (src == invX) { m = mTmp; } else { m = invX; }

    det = src[0][0]*src[1][1]*src[2][2] + src[0][1]*src[1][2]*src[2][0]
        + src[0][2]*src[1][0]*src[2][1] - src[2][0]*src[1][1]*src[0][2]
        - src[1][0]*src[0][1]*src[2][2] - src[0][0]*src[2][1]*src[1][2];

    if (det == 0.0f) return 0;
    det = 1.0f / det;

    m[0][0] =  (src[1][1]*src[2][2] - src[2][1]*src[1][2]) * det;
    m[0][1] = -(src[1][0]*src[2][2] - src[2][0]*src[1][2]) * det;
    m[0][2] =  (src[1][0]*src[2][1] - src[2][0]*src[1][1]) * det;

    m[1][0] = -(src[0][1]*src[2][2] - src[2][1]*src[0][2]) * det;
    m[1][1] =  (src[0][0]*src[2][2] - src[2][0]*src[0][2]) * det;
    m[1][2] = -(src[0][0]*src[2][1] - src[2][0]*src[0][1]) * det;

    m[2][0] =  (src[0][1]*src[1][2] - src[1][1]*src[0][2]) * det;
    m[2][1] = -(src[0][0]*src[1][2] - src[1][0]*src[0][2]) * det;
    m[2][2] =  (src[0][0]*src[1][1] - src[1][0]*src[0][1]) * det;

    m[0][3] = 0.0f; m[1][3] = 0.0f; m[2][3] = 0.0f;

    if (m == mTmp) { oracle_C_MTXCopy(mTmp, invX); }
    return 1;
}

static void oracle_C_MTXRotTrig(oracle_Mtx m, char axis, oracle_f32 sinA, oracle_f32 cosA)
{
    switch (axis) {
    case 'x': case 'X':
        m[0][0]=1.0f; m[0][1]=0.0f;  m[0][2]=0.0f;  m[0][3]=0.0f;
        m[1][0]=0.0f; m[1][1]=cosA;  m[1][2]=-sinA; m[1][3]=0.0f;
        m[2][0]=0.0f; m[2][1]=sinA;  m[2][2]=cosA;  m[2][3]=0.0f;
        break;
    case 'y': case 'Y':
        m[0][0]=cosA;  m[0][1]=0.0f; m[0][2]=sinA;  m[0][3]=0.0f;
        m[1][0]=0.0f;  m[1][1]=1.0f; m[1][2]=0.0f;  m[1][3]=0.0f;
        m[2][0]=-sinA; m[2][1]=0.0f; m[2][2]=cosA;  m[2][3]=0.0f;
        break;
    case 'z': case 'Z':
        m[0][0]=cosA;  m[0][1]=-sinA; m[0][2]=0.0f; m[0][3]=0.0f;
        m[1][0]=sinA;  m[1][1]=cosA;  m[1][2]=0.0f; m[1][3]=0.0f;
        m[2][0]=0.0f;  m[2][1]=0.0f;  m[2][2]=1.0f; m[2][3]=0.0f;
        break;
    default: break;
    }
}

static void oracle_C_MTXRotRad(oracle_Mtx m, char axis, oracle_f32 rad)
{
    oracle_f32 sinA = sinf(rad);
    oracle_f32 cosA = cosf(rad);
    oracle_C_MTXRotTrig(m, axis, sinA, cosA);
}

static void oracle_C_MTXRotAxisRad(oracle_Mtx m, const oracle_Vec *axis, oracle_f32 rad)
{
    oracle_Vec vN;
    oracle_f32 s, c, t;
    oracle_f32 x, y, z;
    oracle_f32 xSq, ySq, zSq;

    s = sinf(rad);
    c = cosf(rad);
    t = 1.0f - c;

    oracle_C_VECNormalize(axis, &vN);

    x = vN.x; y = vN.y; z = vN.z;
    xSq = x*x; ySq = y*y; zSq = z*z;

    m[0][0] = (t*xSq) + c;         m[0][1] = (t*x*y) - (s*z);   m[0][2] = (t*x*z) + (s*y); m[0][3] = 0.0f;
    m[1][0] = (t*x*y) + (s*z);     m[1][1] = (t*ySq) + c;       m[1][2] = (t*y*z) - (s*x); m[1][3] = 0.0f;
    m[2][0] = (t*x*z) - (s*y);     m[2][1] = (t*y*z) + (s*x);   m[2][2] = (t*zSq) + c;     m[2][3] = 0.0f;
}

static void oracle_C_MTXTrans(oracle_Mtx m, oracle_f32 xT, oracle_f32 yT, oracle_f32 zT)
{
    m[0][0]=1.0f; m[0][1]=0.0f; m[0][2]=0.0f; m[0][3]=xT;
    m[1][0]=0.0f; m[1][1]=1.0f; m[1][2]=0.0f; m[1][3]=yT;
    m[2][0]=0.0f; m[2][1]=0.0f; m[2][2]=1.0f; m[2][3]=zT;
}

static void oracle_C_MTXTransApply(const oracle_Mtx src, oracle_Mtx dst,
                                   oracle_f32 xT, oracle_f32 yT, oracle_f32 zT)
{
    if (src != dst) {
        dst[0][0]=src[0][0]; dst[0][1]=src[0][1]; dst[0][2]=src[0][2];
        dst[1][0]=src[1][0]; dst[1][1]=src[1][1]; dst[1][2]=src[1][2];
        dst[2][0]=src[2][0]; dst[2][1]=src[2][1]; dst[2][2]=src[2][2];
    }
    dst[0][3] = src[0][3] + xT;
    dst[1][3] = src[1][3] + yT;
    dst[2][3] = src[2][3] + zT;
}

static void oracle_C_MTXScale(oracle_Mtx m, oracle_f32 xS, oracle_f32 yS, oracle_f32 zS)
{
    m[0][0]=xS;   m[0][1]=0.0f; m[0][2]=0.0f; m[0][3]=0.0f;
    m[1][0]=0.0f; m[1][1]=yS;   m[1][2]=0.0f; m[1][3]=0.0f;
    m[2][0]=0.0f; m[2][1]=0.0f; m[2][2]=zS;   m[2][3]=0.0f;
}

static void oracle_C_MTXScaleApply(const oracle_Mtx src, oracle_Mtx dst,
                                   oracle_f32 xS, oracle_f32 yS, oracle_f32 zS)
{
    dst[0][0]=src[0][0]*xS; dst[0][1]=src[0][1]*xS;
    dst[0][2]=src[0][2]*xS; dst[0][3]=src[0][3]*xS;
    dst[1][0]=src[1][0]*yS; dst[1][1]=src[1][1]*yS;
    dst[1][2]=src[1][2]*yS; dst[1][3]=src[1][3]*yS;
    dst[2][0]=src[2][0]*zS; dst[2][1]=src[2][1]*zS;
    dst[2][2]=src[2][2]*zS; dst[2][3]=src[2][3]*zS;
}

static void oracle_C_MTXQuat(oracle_Mtx m, const oracle_Quaternion *q)
{
    oracle_f32 s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;
    s = 2.0f / (q->x*q->x + q->y*q->y + q->z*q->z + q->w*q->w);

    xs = q->x*s; ys = q->y*s; zs = q->z*s;
    wx = q->w*xs; wy = q->w*ys; wz = q->w*zs;
    xx = q->x*xs; xy = q->x*ys; xz = q->x*zs;
    yy = q->y*ys; yz = q->y*zs; zz = q->z*zs;

    m[0][0] = 1.0f-(yy+zz); m[0][1] = xy-wz;         m[0][2] = xz+wy;         m[0][3] = 0.0f;
    m[1][0] = xy+wz;         m[1][1] = 1.0f-(xx+zz);  m[1][2] = yz-wx;         m[1][3] = 0.0f;
    m[2][0] = xz-wy;         m[2][1] = yz+wx;          m[2][2] = 1.0f-(xx+yy);  m[2][3] = 0.0f;
}

static void oracle_C_MTXReflect(oracle_Mtx m, const oracle_Vec *p, const oracle_Vec *n)
{
    oracle_f32 vxy, vxz, vyz, pdotn;

    vxy = -2.0f * n->x * n->y;
    vxz = -2.0f * n->x * n->z;
    vyz = -2.0f * n->y * n->z;
    pdotn = 2.0f * oracle_C_VECDotProduct(p, n);

    m[0][0] = 1.0f - 2.0f*n->x*n->x;  m[0][1] = vxy;                      m[0][2] = vxz;                      m[0][3] = pdotn*n->x;
    m[1][0] = vxy;                      m[1][1] = 1.0f - 2.0f*n->y*n->y;   m[1][2] = vyz;                      m[1][3] = pdotn*n->y;
    m[2][0] = vxz;                      m[2][1] = vyz;                      m[2][2] = 1.0f - 2.0f*n->z*n->z;   m[2][3] = pdotn*n->z;
}

static void oracle_C_MTXLookAt(oracle_Mtx m, const oracle_Vec *camPos,
                                const oracle_Vec *camUp, const oracle_Vec *target)
{
    oracle_Vec vLook, vRight, vUp;

    vLook.x = camPos->x - target->x;
    vLook.y = camPos->y - target->y;
    vLook.z = camPos->z - target->z;
    oracle_C_VECNormalize(&vLook, &vLook);
    oracle_C_VECCrossProduct(camUp, &vLook, &vRight);
    oracle_C_VECNormalize(&vRight, &vRight);
    oracle_C_VECCrossProduct(&vLook, &vRight, &vUp);

    m[0][0] = vRight.x; m[0][1] = vRight.y; m[0][2] = vRight.z;
    m[0][3] = -(camPos->x*vRight.x + camPos->y*vRight.y + camPos->z*vRight.z);
    m[1][0] = vUp.x;    m[1][1] = vUp.y;    m[1][2] = vUp.z;
    m[1][3] = -(camPos->x*vUp.x + camPos->y*vUp.y + camPos->z*vUp.z);
    m[2][0] = vLook.x;  m[2][1] = vLook.y;  m[2][2] = vLook.z;
    m[2][3] = -(camPos->x*vLook.x + camPos->y*vLook.y + camPos->z*vLook.z);
}

static void oracle_C_MTXLightFrustum(oracle_Mtx m, float t, float b, float l, float r,
                                     float n, float scaleS, float scaleT,
                                     float transS, float transT)
{
    oracle_f32 tmp;
    tmp = 1.0f / (r - l);
    m[0][0] = ((2*n)*tmp) * scaleS;
    m[0][1] = 0.0f;
    m[0][2] = (((r+l)*tmp) * scaleS) - transS;
    m[0][3] = 0.0f;

    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = ((2*n)*tmp) * scaleT;
    m[1][2] = (((t+b)*tmp) * scaleT) - transT;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = -1.0f; m[2][3] = 0.0f;
}

static void oracle_C_MTXLightPerspective(oracle_Mtx m, oracle_f32 fovY, oracle_f32 aspect,
                                         float scaleS, float scaleT,
                                         float transS, float transT)
{
    oracle_f32 angle = fovY * 0.5f;
    oracle_f32 cot;
    angle = oracle_MTXDegToRad(angle);
    cot = 1.0f / tanf(angle);

    m[0][0] = (cot / aspect) * scaleS;
    m[0][1] = 0.0f;
    m[0][2] = -transS;
    m[0][3] = 0.0f;

    m[1][0] = 0.0f;
    m[1][1] = cot * scaleT;
    m[1][2] = -transT;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = -1.0f; m[2][3] = 0.0f;
}

static void oracle_C_MTXLightOrtho(oracle_Mtx m, oracle_f32 t, oracle_f32 b,
                                   oracle_f32 l, oracle_f32 r,
                                   float scaleS, float scaleT,
                                   float transS, float transT)
{
    oracle_f32 tmp;
    tmp = 1.0f / (r - l);
    m[0][0] = 2.0f * tmp * scaleS;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = (-(r+l)*tmp) * scaleS + transS;

    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = (2.0f*tmp) * scaleT;
    m[1][2] = 0.0f;
    m[1][3] = (-(t+b)*tmp) * scaleT + transT;

    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 0.0f; m[2][3] = 1.0f;
}

/* --- MTXMultVec (mtxvec.c — decomp only has PS* asm, trivial C impl) --- */
static void oracle_C_MTXMultVec(const oracle_Mtx m, const oracle_Vec *src, oracle_Vec *dst)
{
    oracle_Vec tmp;
    tmp.x = m[0][0]*src->x + m[0][1]*src->y + m[0][2]*src->z + m[0][3];
    tmp.y = m[1][0]*src->x + m[1][1]*src->y + m[1][2]*src->z + m[1][3];
    tmp.z = m[2][0]*src->x + m[2][1]*src->y + m[2][2]*src->z + m[2][3];
    *dst = tmp;
}

static void oracle_C_MTXMultVecSR(const oracle_Mtx m, const oracle_Vec *src, oracle_Vec *dst)
{
    oracle_Vec tmp;
    tmp.x = m[0][0]*src->x + m[0][1]*src->y + m[0][2]*src->z;
    tmp.y = m[1][0]*src->x + m[1][1]*src->y + m[1][2]*src->z;
    tmp.z = m[2][0]*src->x + m[2][1]*src->y + m[2][2]*src->z;
    *dst = tmp;
}

/* ================================================================== */
/*  MTX44 — 4x4 matrix functions (decomp mtx44.c, exact copy)         */
/* ================================================================== */

static void oracle_C_MTXFrustum(oracle_Mtx44 m, oracle_f32 t, oracle_f32 b,
                                oracle_f32 l, oracle_f32 r, oracle_f32 n, oracle_f32 f)
{
    oracle_f32 tmp = 1.0f / (r - l);
    m[0][0] = (2*n)*tmp; m[0][1] = 0.0f; m[0][2] = (r+l)*tmp; m[0][3] = 0.0f;
    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f; m[1][1] = (2*n)*tmp; m[1][2] = (t+b)*tmp; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(n)*tmp; m[2][3] = -(f*n)*tmp;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = -1.0f; m[3][3] = 0.0f;
}

static void oracle_C_MTXPerspective(oracle_Mtx44 m, oracle_f32 fovY, oracle_f32 aspect,
                                    oracle_f32 n, oracle_f32 f)
{
    oracle_f32 angle = fovY * 0.5f;
    oracle_f32 cot, tmp;
    angle = oracle_MTXDegToRad(angle);
    cot = 1.0f / tanf(angle);

    m[0][0] = cot/aspect; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = cot; m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(n)*tmp; m[2][3] = -(f*n)*tmp;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = -1.0f; m[3][3] = 0.0f;
}

static void oracle_C_MTXOrtho(oracle_Mtx44 m, oracle_f32 t, oracle_f32 b,
                              oracle_f32 l, oracle_f32 r, oracle_f32 n, oracle_f32 f)
{
    oracle_f32 tmp = 1.0f / (r - l);
    m[0][0] = 2.0f*tmp; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = -(r+l)*tmp;
    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f; m[1][1] = 2.0f*tmp; m[1][2] = 0.0f; m[1][3] = -(t+b)*tmp;
    m[2][0] = 0.0f; m[2][1] = 0.0f;
    tmp = 1.0f / (f - n);
    m[2][2] = -(1.0f)*tmp; m[2][3] = -(f)*tmp;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
}

/* ================================================================== */
/*  QUAT — Quaternion functions (decomp quat.c, exact copy)            */
/* ================================================================== */

static void oracle_C_QUATAdd(const oracle_Quaternion *p, const oracle_Quaternion *q,
                             oracle_Quaternion *r)
{
    r->x = p->x + q->x;
    r->y = p->y + q->y;
    r->z = p->z + q->z;
    r->w = p->w + q->w;
}

static void oracle_C_QUATRotAxisRad(oracle_Quaternion *q, const oracle_Vec *axis, oracle_f32 rad)
{
    oracle_f32 tmp, tmp2, tmp3;
    oracle_Vec dst;

    tmp = rad;
    oracle_C_VECNormalize(axis, &dst);

    tmp2 = tmp * 0.5f;
    tmp3 = sinf(tmp * 0.5f);
    tmp = tmp3;
    tmp3 = cosf(tmp2);

    q->x = tmp * dst.x;
    q->y = tmp * dst.y;
    q->z = tmp * dst.z;
    q->w = tmp3;
}

static void oracle_C_QUATMtx(oracle_Quaternion *r, const oracle_Mtx m)
{
    oracle_f32 vv0, vv1;
    oracle_s32 i, j, k;
    oracle_s32 idx[3] = { 1, 2, 0 };
    oracle_f32 vec[3];

    vv0 = m[0][0] + m[1][1] + m[2][2];
    if (vv0 > 0.0f) {
        vv1 = sqrtf(vv0 + 1.0f);
        r->w = vv1 * 0.5f;
        vv1 = 0.5f / vv1;
        r->x = (m[2][1] - m[1][2]) * vv1;
        r->y = (m[0][2] - m[2][0]) * vv1;
        r->z = (m[1][0] - m[0][1]) * vv1;
    } else {
        i = 0;
        if (m[1][1] > m[0][0]) i = 1;
        if (m[2][2] > m[i][i]) i = 2;
        j = idx[i];
        k = idx[j];
        vv1 = sqrtf((m[i][i] - (m[j][j] + m[k][k])) + 1.0f);
        vec[i] = vv1 * 0.5f;
        if (vv1 != 0.0f) vv1 = 0.5f / vv1;
        r->w = (m[k][j] - m[j][k]) * vv1;
        vec[j] = (m[i][j] + m[j][i]) * vv1;
        vec[k] = (m[i][k] + m[k][i]) * vv1;
        r->x = vec[0];
        r->y = vec[1];
        r->z = vec[2];
    }
}

/* --- C_QUATMultiply (decomp PS asm only, trivial C implementation) --- */
static void oracle_C_QUATMultiply(const oracle_Quaternion *a, const oracle_Quaternion *b,
                                  oracle_Quaternion *ab)
{
    oracle_Quaternion tmp;
    tmp.x = a->w*b->x + a->x*b->w + a->y*b->z - a->z*b->y;
    tmp.y = a->w*b->y - a->x*b->z + a->y*b->w + a->z*b->x;
    tmp.z = a->w*b->z + a->x*b->y - a->y*b->x + a->z*b->w;
    tmp.w = a->w*b->w - a->x*b->x - a->y*b->y - a->z*b->z;
    *ab = tmp;
}

/* --- C_QUATNormalize (decomp PS asm only, trivial C implementation) --- */
static void oracle_C_QUATNormalize(const oracle_Quaternion *src, oracle_Quaternion *unit)
{
    oracle_f32 dot = src->x*src->x + src->y*src->y + src->z*src->z + src->w*src->w;
    if (dot < 0.00001f) {
        unit->x = unit->y = unit->z = unit->w = 0.0f;
        return;
    }
    oracle_f32 inv = 1.0f / sqrtf(dot);
    unit->x = src->x * inv;
    unit->y = src->y * inv;
    unit->z = src->z * inv;
    unit->w = src->w * inv;
}

/* --- C_QUATInverse (decomp PS asm only, trivial C implementation) --- */
static void oracle_C_QUATInverse(const oracle_Quaternion *src, oracle_Quaternion *inv)
{
    oracle_f32 dot = src->x*src->x + src->y*src->y + src->z*src->z + src->w*src->w;
    oracle_f32 invDot;
    if (dot <= 0.0f) {
        invDot = 1.0f;
    } else {
        invDot = 1.0f / dot;
    }
    inv->x = -src->x * invDot;
    inv->y = -src->y * invDot;
    inv->z = -src->z * invDot;
    inv->w =  src->w * invDot;
}

static void oracle_C_QUATSlerp(const oracle_Quaternion *p, const oracle_Quaternion *q,
                               oracle_Quaternion *r, oracle_f32 t)
{
    oracle_f32 ratioA, ratioB;
    oracle_f32 value = 1.0f;
    oracle_f32 cosHalfTheta = p->x*q->x + p->y*q->y + p->z*q->z + p->w*q->w;

    if (cosHalfTheta < 0.0f) {
        cosHalfTheta = -cosHalfTheta;
        value = -value;
    }

    if (cosHalfTheta <= 0.9999899864196777f) {
        oracle_f32 halfTheta = acosf(cosHalfTheta);
        oracle_f32 sinHalfTheta = sinf(halfTheta);
        ratioA = sinf((1.0f - t) * halfTheta) / sinHalfTheta;
        ratioB = sinf(t * halfTheta) / sinHalfTheta;
        value *= ratioB;
    } else {
        ratioA = 1.0f - t;
        value *= t;
    }

    r->x = ratioA*p->x + value*q->x;
    r->y = ratioA*p->y + value*q->y;
    r->z = ratioA*p->z + value*q->z;
    r->w = ratioA*p->w + value*q->w;
}
