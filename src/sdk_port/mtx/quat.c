/*
 * sdk_port/mtx/quat.c — Host-side port of GC SDK quaternion functions.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/mtx/quat.c
 * Exact copy of all C_QUAT* functions from the decomp.
 */
#include <math.h>
#include "sdk_port_mtx_types.h"

/* Forward declarations for vec functions used here */
extern void C_VECNormalize(const Vec *src, Vec *unit);

void C_QUATAdd(const Quaternion *p, const Quaternion *q, Quaternion *r)
{
    r->x = p->x + q->x;
    r->y = p->y + q->y;
    r->z = p->z + q->z;
    r->w = p->w + q->w;
}

void C_QUATRotAxisRad(Quaternion *q, const Vec *axis, f32 rad)
{
    f32 tmp, tmp2, tmp3;
    Vec dst;

    tmp = rad;
    C_VECNormalize(axis, &dst);

    tmp2 = tmp * 0.5f;
    tmp3 = sinf(tmp * 0.5f);
    tmp = tmp3;
    tmp3 = cosf(tmp2);

    q->x = tmp * dst.x;
    q->y = tmp * dst.y;
    q->z = tmp * dst.z;
    q->w = tmp3;
}

void C_QUATMtx(Quaternion *r, const Mtx m)
{
    f32 vv0, vv1;
    s32 i, j, k;
    s32 idx[3] = { 1, 2, 0 };
    f32 vec[3];

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

void C_QUATSlerp(const Quaternion *p, const Quaternion *q, Quaternion *r, f32 t)
{
    f32 ratioA, ratioB;
    f32 value = 1.0f;
    f32 cosHalfTheta = p->x*q->x + p->y*q->y + p->z*q->z + p->w*q->w;

    if (cosHalfTheta < 0.0f) {
        cosHalfTheta = -cosHalfTheta;
        value = -value;
    }

    if (cosHalfTheta <= 0.9999899864196777f) {
        f32 halfTheta = acosf(cosHalfTheta);
        f32 sinHalfTheta = sinf(halfTheta);
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
