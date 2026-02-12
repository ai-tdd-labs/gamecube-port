/*
 * sdk_port/mtx/mtx.c — Host-side port of GC SDK matrix functions.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/mtx/mtx.c
 * Exact copy of all C_* functions. No big-endian emulation needed
 * (pure float math, no pointer-size dependencies).
 */
#include <math.h>
#include "sdk_port_mtx_types.h"

/* ================================================================== */
/*  Core matrix operations                                             */
/* ================================================================== */

void C_MTXIdentity(Mtx mtx)
{
    mtx[0][0] = 1.0f; mtx[0][1] = 0.0f; mtx[0][2] = 0.0f; mtx[0][3] = 0.0f;
    mtx[1][0] = 0.0f; mtx[1][1] = 1.0f; mtx[1][2] = 0.0f; mtx[1][3] = 0.0f;
    mtx[2][0] = 0.0f; mtx[2][1] = 0.0f; mtx[2][2] = 1.0f; mtx[2][3] = 0.0f;
}

/* Paired-single variant aliases to the same observable output. */
void PSMTXIdentity(Mtx mtx)
{
    C_MTXIdentity(mtx);
}

void C_MTXCopy(const Mtx src, Mtx dst)
{
    if (src == dst) return;

    dst[0][0] = src[0][0]; dst[0][1] = src[0][1];
    dst[0][2] = src[0][2]; dst[0][3] = src[0][3];
    dst[1][0] = src[1][0]; dst[1][1] = src[1][1];
    dst[1][2] = src[1][2]; dst[1][3] = src[1][3];
    dst[2][0] = src[2][0]; dst[2][1] = src[2][1];
    dst[2][2] = src[2][2]; dst[2][3] = src[2][3];
}

void C_MTXConcat(const Mtx a, const Mtx b, Mtx ab)
{
    Mtx mTmp;
    MtxPtr m;

    if ((ab == a) || (ab == b)) { m = mTmp; } else { m = ab; }

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

    if (m == mTmp) { C_MTXCopy(mTmp, ab); }
}

void C_MTXConcatArray(const Mtx a, const Mtx *srcBase, Mtx *dstBase, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) {
        C_MTXConcat(a, *srcBase, *dstBase);
        srcBase++;
        dstBase++;
    }
}

void C_MTXTranspose(const Mtx src, Mtx xPose)
{
    Mtx mTmp;
    MtxPtr m;

    if (src == xPose) { m = mTmp; } else { m = xPose; }

    m[0][0] = src[0][0]; m[0][1] = src[1][0]; m[0][2] = src[2][0]; m[0][3] = 0.0f;
    m[1][0] = src[0][1]; m[1][1] = src[1][1]; m[1][2] = src[2][1]; m[1][3] = 0.0f;
    m[2][0] = src[0][2]; m[2][1] = src[1][2]; m[2][2] = src[2][2]; m[2][3] = 0.0f;

    if (m == mTmp) { C_MTXCopy(mTmp, xPose); }
}

u32 C_MTXInverse(const Mtx src, Mtx inv)
{
    Mtx mTmp;
    MtxPtr m;
    f32 det;

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

    if (m == mTmp) { C_MTXCopy(mTmp, inv); }
    return 1;
}

u32 C_MTXInvXpose(const Mtx src, Mtx invX)
{
    Mtx mTmp;
    MtxPtr m;
    f32 det;

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

    if (m == mTmp) { C_MTXCopy(mTmp, invX); }
    return 1;
}

/* ================================================================== */
/*  Rotation                                                           */
/* ================================================================== */

void C_MTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA)
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

void C_MTXRotRad(Mtx m, char axis, f32 rad)
{
    f32 sinA = sinf(rad);
    f32 cosA = cosf(rad);
    C_MTXRotTrig(m, axis, sinA, cosA);
}

void C_MTXRotAxisRad(Mtx m, const Vec *axis, f32 rad)
{
    Vec vN;
    f32 s, c, t, x, y, z, xSq, ySq, zSq;

    s = sinf(rad);
    c = cosf(rad);
    t = 1.0f - c;

    C_VECNormalize(axis, &vN);

    x = vN.x; y = vN.y; z = vN.z;
    xSq = x*x; ySq = y*y; zSq = z*z;

    m[0][0] = (t*xSq)+c;       m[0][1] = (t*x*y)-(s*z); m[0][2] = (t*x*z)+(s*y); m[0][3] = 0.0f;
    m[1][0] = (t*x*y)+(s*z);   m[1][1] = (t*ySq)+c;     m[1][2] = (t*y*z)-(s*x); m[1][3] = 0.0f;
    m[2][0] = (t*x*z)-(s*y);   m[2][1] = (t*y*z)+(s*x); m[2][2] = (t*zSq)+c;     m[2][3] = 0.0f;
}

/* ================================================================== */
/*  Translation / Scale                                                */
/* ================================================================== */

void C_MTXTrans(Mtx m, f32 xT, f32 yT, f32 zT)
{
    m[0][0]=1.0f; m[0][1]=0.0f; m[0][2]=0.0f; m[0][3]=xT;
    m[1][0]=0.0f; m[1][1]=1.0f; m[1][2]=0.0f; m[1][3]=yT;
    m[2][0]=0.0f; m[2][1]=0.0f; m[2][2]=1.0f; m[2][3]=zT;
}

void C_MTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT)
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

void C_MTXScale(Mtx m, f32 xS, f32 yS, f32 zS)
{
    m[0][0]=xS;   m[0][1]=0.0f; m[0][2]=0.0f; m[0][3]=0.0f;
    m[1][0]=0.0f; m[1][1]=yS;   m[1][2]=0.0f; m[1][3]=0.0f;
    m[2][0]=0.0f; m[2][1]=0.0f; m[2][2]=zS;   m[2][3]=0.0f;
}

void C_MTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS)
{
    dst[0][0]=src[0][0]*xS; dst[0][1]=src[0][1]*xS;
    dst[0][2]=src[0][2]*xS; dst[0][3]=src[0][3]*xS;
    dst[1][0]=src[1][0]*yS; dst[1][1]=src[1][1]*yS;
    dst[1][2]=src[1][2]*yS; dst[1][3]=src[1][3]*yS;
    dst[2][0]=src[2][0]*zS; dst[2][1]=src[2][1]*zS;
    dst[2][2]=src[2][2]*zS; dst[2][3]=src[2][3]*zS;
}

/* ================================================================== */
/*  Quaternion -> Matrix, Reflect, LookAt                              */
/* ================================================================== */

void C_MTXQuat(Mtx m, const Quaternion *q)
{
    f32 s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz;
    s = 2.0f / (q->x*q->x + q->y*q->y + q->z*q->z + q->w*q->w);

    xs = q->x*s; ys = q->y*s; zs = q->z*s;
    wx = q->w*xs; wy = q->w*ys; wz = q->w*zs;
    xx = q->x*xs; xy = q->x*ys; xz = q->x*zs;
    yy = q->y*ys; yz = q->y*zs; zz = q->z*zs;

    m[0][0] = 1.0f-(yy+zz); m[0][1] = xy-wz;        m[0][2] = xz+wy;        m[0][3] = 0.0f;
    m[1][0] = xy+wz;        m[1][1] = 1.0f-(xx+zz); m[1][2] = yz-wx;        m[1][3] = 0.0f;
    m[2][0] = xz-wy;        m[2][1] = yz+wx;         m[2][2] = 1.0f-(xx+yy); m[2][3] = 0.0f;
}

void C_MTXReflect(Mtx m, const Vec *p, const Vec *n)
{
    f32 vxy, vxz, vyz, pdotn;

    vxy = -2.0f * n->x * n->y;
    vxz = -2.0f * n->x * n->z;
    vyz = -2.0f * n->y * n->z;
    pdotn = 2.0f * C_VECDotProduct(p, n);

    m[0][0] = 1.0f - 2.0f*n->x*n->x; m[0][1] = vxy;                     m[0][2] = vxz;                     m[0][3] = pdotn*n->x;
    m[1][0] = vxy;                     m[1][1] = 1.0f - 2.0f*n->y*n->y;  m[1][2] = vyz;                     m[1][3] = pdotn*n->y;
    m[2][0] = vxz;                     m[2][1] = vyz;                     m[2][2] = 1.0f - 2.0f*n->z*n->z;  m[2][3] = pdotn*n->z;
}

void C_MTXLookAt(Mtx m, const Vec *camPos, const Vec *camUp, const Vec *target)
{
    Vec vLook, vRight, vUp;

    vLook.x = camPos->x - target->x;
    vLook.y = camPos->y - target->y;
    vLook.z = camPos->z - target->z;
    C_VECNormalize(&vLook, &vLook);
    C_VECCrossProduct(camUp, &vLook, &vRight);
    C_VECNormalize(&vRight, &vRight);
    C_VECCrossProduct(&vLook, &vRight, &vUp);

    m[0][0] = vRight.x; m[0][1] = vRight.y; m[0][2] = vRight.z;
    m[0][3] = -(camPos->x*vRight.x + camPos->y*vRight.y + camPos->z*vRight.z);
    m[1][0] = vUp.x;    m[1][1] = vUp.y;    m[1][2] = vUp.z;
    m[1][3] = -(camPos->x*vUp.x + camPos->y*vUp.y + camPos->z*vUp.z);
    m[2][0] = vLook.x;  m[2][1] = vLook.y;  m[2][2] = vLook.z;
    m[2][3] = -(camPos->x*vLook.x + camPos->y*vLook.y + camPos->z*vLook.z);
}

/* ================================================================== */
/*  Light matrices                                                     */
/* ================================================================== */

void C_MTXLightFrustum(Mtx m, float t, float b, float l, float r,
                       float n, float scaleS, float scaleT,
                       float transS, float transT)
{
    f32 tmp;
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

void C_MTXLightPerspective(Mtx m, f32 fovY, f32 aspect, float scaleS, float scaleT,
                           float transS, float transT)
{
    f32 angle = fovY * 0.5f;
    f32 cot;
    angle = MTXDegToRad(angle);
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

void C_MTXLightOrtho(Mtx m, f32 t, f32 b, f32 l, f32 r,
                     float scaleS, float scaleT, float transS, float transT)
{
    f32 tmp;
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

/* ================================================================== */
/*  MTXMultVec (decomp only has PS* asm; trivial C implementation)     */
/* ================================================================== */

void C_MTXMultVec(const Mtx m, const Vec *src, Vec *dst)
{
    Vec tmp;
    tmp.x = m[0][0]*src->x + m[0][1]*src->y + m[0][2]*src->z + m[0][3];
    tmp.y = m[1][0]*src->x + m[1][1]*src->y + m[1][2]*src->z + m[1][3];
    tmp.z = m[2][0]*src->x + m[2][1]*src->y + m[2][2]*src->z + m[2][3];
    *dst = tmp;
}

void C_MTXMultVecSR(const Mtx m, const Vec *src, Vec *dst)
{
    Vec tmp;
    tmp.x = m[0][0]*src->x + m[0][1]*src->y + m[0][2]*src->z;
    tmp.y = m[1][0]*src->x + m[1][1]*src->y + m[1][2]*src->z;
    tmp.z = m[2][0]*src->x + m[2][1]*src->y + m[2][2]*src->z;
    *dst = tmp;
}

/* ================================================================== */
/*  PSMTXMultVecArray (PS* ASM in decomp; C loop over C_MTXMultVec)    */
/* ================================================================== */

typedef f32 ROMtx[3][4];

void PSMTXMultVecArray(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++) {
        C_MTXMultVec(m, &srcBase[i], &dstBase[i]);
    }
}

/* ================================================================== */
/*  PSMTXReorder (PS* ASM in decomp; transpose Mtx to column-major)    */
/* ================================================================== */

void PSMTXReorder(const Mtx src, ROMtx dest)
{
    /* Row-major Mtx → column-major ROMtx:
     * dest layout: [m00 m10 m20 | m01 m11 m21 | m02 m12 m22 | m03 m13 m23] */
    dest[0][0] = src[0][0]; dest[0][1] = src[1][0]; dest[0][2] = src[2][0];
    dest[0][3] = src[0][1]; dest[1][0] = src[1][1]; dest[1][1] = src[2][1];
    dest[1][2] = src[0][2]; dest[1][3] = src[1][2]; dest[2][0] = src[2][2];
    dest[2][1] = src[0][3]; dest[2][2] = src[1][3]; dest[2][3] = src[2][3];
}

/* ================================================================== */
/*  PSMTXROMultVecArray (PS* ASM; multiply reordered matrix * vecs)     */
/* ================================================================== */

void PSMTXROMultVecArray(const ROMtx m, const Vec *srcBase, Vec *dstBase, u32 count)
{
    /* ROMtx column-major:
     * col0 = m[0][0..2], col1 = m[0][3]..m[1][1], col2 = m[1][2]..m[2][0],
     * translate = m[2][1..3] */
    const f32 r00 = m[0][0], r10 = m[0][1], r20 = m[0][2];
    const f32 r01 = m[0][3], r11 = m[1][0], r21 = m[1][1];
    const f32 r02 = m[1][2], r12 = m[1][3], r22 = m[2][0];
    const f32 r03 = m[2][1], r13 = m[2][2], r23 = m[2][3];

    u32 i;
    for (i = 0; i < count; i++) {
        f32 x = srcBase[i].x, y = srcBase[i].y, z = srcBase[i].z;
        dstBase[i].x = r00*x + r01*y + r02*z + r03;
        dstBase[i].y = r10*x + r11*y + r12*z + r13;
        dstBase[i].z = r20*x + r21*y + r22*z + r23;
    }
}
