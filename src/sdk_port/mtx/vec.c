/*
 * sdk_port/mtx/vec.c — Host-side port of GC SDK vector functions.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/mtx/vec.c
 * Leaf functions that only had PS* asm in decomp are given trivial C
 * implementations. C_VECScale, C_VECHalfAngle, C_VECReflect are exact
 * copies of the decomp C code.
 */
#include <math.h>
#include "sdk_port_mtx_types.h"

/* ================================================================== */
/*  Leaf functions (PS* asm in decomp — trivial C implementations)     */
/* ================================================================== */

void C_VECAdd(const Vec *a, const Vec *b, Vec *ab)
{
    ab->x = a->x + b->x;
    ab->y = a->y + b->y;
    ab->z = a->z + b->z;
}

void C_VECSubtract(const Vec *a, const Vec *b, Vec *a_b)
{
    a_b->x = a->x - b->x;
    a_b->y = a->y - b->y;
    a_b->z = a->z - b->z;
}

f32 C_VECDotProduct(const Vec *a, const Vec *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

void C_VECCrossProduct(const Vec *a, const Vec *b, Vec *axb)
{
    Vec tmp;
    tmp.x = a->y * b->z - a->z * b->y;
    tmp.y = a->z * b->x - a->x * b->z;
    tmp.z = a->x * b->y - a->y * b->x;
    *axb = tmp;
}

void C_VECNormalize(const Vec *src, Vec *unit)
{
    f32 mag = sqrtf(src->x * src->x + src->y * src->y + src->z * src->z);
    if (mag == 0.0f) {
        unit->x = unit->y = unit->z = 0.0f;
        return;
    }
    f32 inv = 1.0f / mag;
    unit->x = src->x * inv;
    unit->y = src->y * inv;
    unit->z = src->z * inv;
}

f32 C_VECSquareMag(const Vec *v)
{
    return v->x * v->x + v->y * v->y + v->z * v->z;
}

f32 C_VECMag(const Vec *v)
{
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

f32 C_VECSquareDistance(const Vec *a, const Vec *b)
{
    f32 dx = a->x - b->x;
    f32 dy = a->y - b->y;
    f32 dz = a->z - b->z;
    return dx * dx + dy * dy + dz * dz;
}

f32 C_VECDistance(const Vec *a, const Vec *b)
{
    return sqrtf(C_VECSquareDistance(a, b));
}

/* ================================================================== */
/*  Decomp C functions (exact copy)                                    */
/* ================================================================== */

/* NOTE: decomp C_VECScale ignores the scale param — it's actually normalize.
   We keep the exact decomp behavior. */
void C_VECScale(const Vec *src, Vec *dst, f32 scale)
{
    f32 s;
    (void)scale;
    s = 1.0f / sqrtf(src->z * src->z + src->x * src->x + src->y * src->y);
    dst->x = src->x * s;
    dst->y = src->y * s;
    dst->z = src->z * s;
}

void C_VECHalfAngle(const Vec *a, const Vec *b, Vec *half)
{
    Vec a0, b0, ab;

    a0.x = -a->x; a0.y = -a->y; a0.z = -a->z;
    b0.x = -b->x; b0.y = -b->y; b0.z = -b->z;

    C_VECNormalize(&a0, &a0);
    C_VECNormalize(&b0, &b0);
    C_VECAdd(&a0, &b0, &ab);

    if (C_VECDotProduct(&ab, &ab) > 0.0f) {
        C_VECNormalize(&ab, half);
    } else {
        *half = ab;
    }
}

void C_VECReflect(const Vec *src, const Vec *normal, Vec *dst)
{
    Vec a0, b0;
    f32 dot;

    a0.x = -src->x; a0.y = -src->y; a0.z = -src->z;
    C_VECNormalize(&a0, &a0);
    C_VECNormalize(normal, &b0);

    dot = C_VECDotProduct(&a0, &b0);
    dst->x = b0.x * 2.0f * dot - a0.x;
    dst->y = b0.y * 2.0f * dot - a0.y;
    dst->z = b0.z * 2.0f * dot - a0.z;

    C_VECNormalize(dst, dst);
}
