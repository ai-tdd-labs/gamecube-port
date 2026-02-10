/*
 * mtx_property_test.c — Property-based parity test for GC SDK MTX/VEC/QUAT.
 *
 * Oracle: decomp C_* functions (in mtx_oracle.h, oracle_ prefix)
 * Port:   sdk_port C_* functions (linked from src/sdk_port/mtx/)
 *
 * For each seed, generates random matrices/vectors/quaternions,
 * calls both oracle and port with identical inputs, and compares
 * outputs with an epsilon tolerance for floating point.
 *
 * Test levels:
 *   L0 (leaves):      VEC functions (Add, Sub, Dot, Cross, Normalize, etc.)
 *   L1 (API):         Individual MTX/QUAT functions
 *   L2 (integration): Chains like LookAt, RotAxisRad, QUATSlerp
 *
 * Usage:
 *   mtx_property_test [--seed=N] [--op=NAME] [--num-runs=N] [--steps=N] [-v]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ---- Oracle (static inline, self-contained) ---- */
#include "mtx_oracle.h"

/* ---- Port declarations ---- */
typedef float f32;
typedef unsigned int u32;
typedef int s32;
typedef f32 Mtx[3][4];
typedef f32 (*MtxPtr)[4];
typedef f32 Mtx44[4][4];
typedef struct { f32 x, y, z; } Vec;
typedef struct { f32 x, y, z, w; } Quaternion;

/* MTX */
extern void C_MTXIdentity(Mtx m);
extern void C_MTXCopy(const Mtx src, Mtx dst);
extern void C_MTXConcat(const Mtx a, const Mtx b, Mtx ab);
extern void C_MTXTranspose(const Mtx src, Mtx xPose);
extern u32  C_MTXInverse(const Mtx src, Mtx inv);
extern u32  C_MTXInvXpose(const Mtx src, Mtx invX);
extern void C_MTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA);
extern void C_MTXRotRad(Mtx m, char axis, f32 rad);
extern void C_MTXRotAxisRad(Mtx m, const Vec *axis, f32 rad);
extern void C_MTXTrans(Mtx m, f32 xT, f32 yT, f32 zT);
extern void C_MTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT);
extern void C_MTXScale(Mtx m, f32 xS, f32 yS, f32 zS);
extern void C_MTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS);
extern void C_MTXQuat(Mtx m, const Quaternion *q);
extern void C_MTXReflect(Mtx m, const Vec *p, const Vec *n);
extern void C_MTXLookAt(Mtx m, const Vec *camPos, const Vec *camUp, const Vec *target);
extern void C_MTXLightFrustum(Mtx m, float t, float b, float l, float r, float n, float scaleS, float scaleT, float transS, float transT);
extern void C_MTXLightPerspective(Mtx m, f32 fovY, f32 aspect, float scaleS, float scaleT, float transS, float transT);
extern void C_MTXLightOrtho(Mtx m, f32 t, f32 b, f32 l, f32 r, float scaleS, float scaleT, float transS, float transT);
extern void C_MTXMultVec(const Mtx m, const Vec *src, Vec *dst);
extern void C_MTXMultVecSR(const Mtx m, const Vec *src, Vec *dst);

/* MTX44 */
extern void C_MTXFrustum(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
extern void C_MTXPerspective(Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f);
extern void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);

/* VEC */
extern void C_VECAdd(const Vec *a, const Vec *b, Vec *ab);
extern void C_VECSubtract(const Vec *a, const Vec *b, Vec *a_b);
extern f32  C_VECDotProduct(const Vec *a, const Vec *b);
extern void C_VECCrossProduct(const Vec *a, const Vec *b, Vec *axb);
extern void C_VECNormalize(const Vec *src, Vec *unit);
extern f32  C_VECSquareMag(const Vec *v);
extern f32  C_VECMag(const Vec *v);
extern f32  C_VECSquareDistance(const Vec *a, const Vec *b);
extern f32  C_VECDistance(const Vec *a, const Vec *b);
extern void C_VECHalfAngle(const Vec *a, const Vec *b, Vec *half);
extern void C_VECReflect(const Vec *src, const Vec *normal, Vec *dst);

/* QUAT */
extern void C_QUATAdd(const Quaternion *p, const Quaternion *q, Quaternion *r);
extern void C_QUATRotAxisRad(Quaternion *q, const Vec *axis, f32 rad);
extern void C_QUATMtx(Quaternion *r, const Mtx m);
extern void C_QUATSlerp(const Quaternion *p, const Quaternion *q, Quaternion *r, f32 t);

/* ================================================================== */
/*  PRNG (xorshift32)                                                  */
/* ================================================================== */

static unsigned int g_rng_state;

static void rng_seed(unsigned int s) { g_rng_state = s ? s : 1; }

static unsigned int rng_next(void)
{
    unsigned int x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return x;
}

/* Random float in [lo, hi] */
static float rng_float(float lo, float hi)
{
    float t = (float)(rng_next() & 0xFFFFFF) / (float)0xFFFFFF;
    return lo + t * (hi - lo);
}

/* Random non-zero vec (for normalize tests) */
static void rng_vec(oracle_Vec *ov, Vec *pv, float lo, float hi)
{
    float x = rng_float(lo, hi);
    float y = rng_float(lo, hi);
    float z = rng_float(lo, hi);
    ov->x = x; ov->y = y; ov->z = z;
    pv->x = x; pv->y = y; pv->z = z;
}

static void rng_vec_nonzero(oracle_Vec *ov, Vec *pv)
{
    do {
        rng_vec(ov, pv, -10.0f, 10.0f);
    } while (ov->x == 0.0f && ov->y == 0.0f && ov->z == 0.0f);
}

static void rng_quat(oracle_Quaternion *oq, Quaternion *pq)
{
    float x = rng_float(-1.0f, 1.0f);
    float y = rng_float(-1.0f, 1.0f);
    float z = rng_float(-1.0f, 1.0f);
    float w = rng_float(-1.0f, 1.0f);
    /* Normalize to unit quaternion */
    float mag = sqrtf(x*x + y*y + z*z + w*w);
    if (mag > 0.0f) { x /= mag; y /= mag; z /= mag; w /= mag; }
    else { x = 0; y = 0; z = 0; w = 1.0f; }
    oq->x = x; oq->y = y; oq->z = z; oq->w = w;
    pq->x = x; pq->y = y; pq->z = z; pq->w = w;
}

static void rng_mtx(oracle_Mtx om, Mtx pm)
{
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++) {
            float v = rng_float(-10.0f, 10.0f);
            om[r][c] = v;
            pm[r][c] = v;
        }
}

/* ================================================================== */
/*  Comparison helpers                                                 */
/* ================================================================== */

#define EPSILON 1e-5f

static int verbose = 0;
static int total_pass = 0;
static int total_fail = 0;

static int float_eq(float a, float b)
{
    if (a == b) return 1;
    float diff = fabsf(a - b);
    float maxab = fabsf(a) > fabsf(b) ? fabsf(a) : fabsf(b);
    if (maxab < 1.0f) maxab = 1.0f;
    return diff / maxab < EPSILON;
}

static int mtx_eq(const oracle_Mtx om, const Mtx pm)
{
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++)
            if (!float_eq(om[r][c], pm[r][c])) return 0;
    return 1;
}

static int mtx44_eq(const oracle_Mtx44 om, const Mtx44 pm)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (!float_eq(om[r][c], pm[r][c])) return 0;
    return 1;
}

static int vec_eq(const oracle_Vec *ov, const Vec *pv)
{
    return float_eq(ov->x, pv->x) && float_eq(ov->y, pv->y) && float_eq(ov->z, pv->z);
}

static int quat_eq(const oracle_Quaternion *oq, const Quaternion *pq)
{
    return float_eq(oq->x, pq->x) && float_eq(oq->y, pq->y)
        && float_eq(oq->z, pq->z) && float_eq(oq->w, pq->w);
}

static void print_mtx(const char *label, const float m[][4], int rows)
{
    printf("  %s:\n", label);
    for (int r = 0; r < rows; r++) {
        printf("    [");
        for (int c = 0; c < 4; c++)
            printf(" %12.6f", m[r][c]);
        printf(" ]\n");
    }
}

static void print_vec(const char *label, float x, float y, float z)
{
    printf("  %s: (%f, %f, %f)\n", label, x, y, z);
}

#define CHECK_MTX(name, om, pm) do { \
    if (mtx_eq(om, pm)) { total_pass++; } \
    else { \
        total_fail++; \
        printf("FAIL [seed=%u] %s\n", seed, name); \
        if (verbose) { print_mtx("oracle", om, 3); print_mtx("port", pm, 3); } \
    } \
} while(0)

#define CHECK_MTX44(name, om, pm) do { \
    if (mtx44_eq(om, pm)) { total_pass++; } \
    else { \
        total_fail++; \
        printf("FAIL [seed=%u] %s\n", seed, name); \
        if (verbose) { print_mtx("oracle", om, 4); print_mtx("port", pm, 4); } \
    } \
} while(0)

#define CHECK_VEC(name, ov, pv) do { \
    if (vec_eq(&(ov), &(pv))) { total_pass++; } \
    else { \
        total_fail++; \
        printf("FAIL [seed=%u] %s\n", seed, name); \
        if (verbose) { \
            print_vec("oracle", (ov).x, (ov).y, (ov).z); \
            print_vec("port", (pv).x, (pv).y, (pv).z); \
        } \
    } \
} while(0)

#define CHECK_FLOAT(name, of, pf) do { \
    if (float_eq(of, pf)) { total_pass++; } \
    else { \
        total_fail++; \
        printf("FAIL [seed=%u] %s: oracle=%f port=%f\n", seed, name, (double)(of), (double)(pf)); \
    } \
} while(0)

#define CHECK_QUAT(name, oq, pq) do { \
    if (quat_eq(&(oq), &(pq))) { total_pass++; } \
    else { \
        total_fail++; \
        printf("FAIL [seed=%u] %s\n", seed, name); \
        if (verbose) { \
            printf("  oracle: (%f, %f, %f, %f)\n", (double)(oq).x, (double)(oq).y, (double)(oq).z, (double)(oq).w); \
            printf("  port:   (%f, %f, %f, %f)\n", (double)(pq).x, (double)(pq).y, (double)(pq).z, (double)(pq).w); \
        } \
    } \
} while(0)

#define CHECK_U32(name, ov, pv) do { \
    if ((ov) == (pv)) { total_pass++; } \
    else { \
        total_fail++; \
        printf("FAIL [seed=%u] %s: oracle=%u port=%u\n", seed, name, (unsigned)(ov), (unsigned)(pv)); \
    } \
} while(0)

/* ================================================================== */
/*  Test suites                                                        */
/* ================================================================== */

static void test_vec_leaves(unsigned int seed)
{
    oracle_Vec oa, ob, ov; Vec pa, pb, pv;

    /* VECAdd */
    rng_vec(&oa, &pa, -10.0f, 10.0f);
    rng_vec(&ob, &pb, -10.0f, 10.0f);
    oracle_C_VECAdd(&oa, &ob, &ov);
    C_VECAdd(&pa, &pb, &pv);
    CHECK_VEC("VECAdd", ov, pv);

    /* VECSubtract */
    rng_vec(&oa, &pa, -10.0f, 10.0f);
    rng_vec(&ob, &pb, -10.0f, 10.0f);
    oracle_C_VECSubtract(&oa, &ob, &ov);
    C_VECSubtract(&pa, &pb, &pv);
    CHECK_VEC("VECSubtract", ov, pv);

    /* VECDotProduct */
    rng_vec(&oa, &pa, -10.0f, 10.0f);
    rng_vec(&ob, &pb, -10.0f, 10.0f);
    float od = oracle_C_VECDotProduct(&oa, &ob);
    float pd = C_VECDotProduct(&pa, &pb);
    CHECK_FLOAT("VECDotProduct", od, pd);

    /* VECCrossProduct */
    rng_vec(&oa, &pa, -10.0f, 10.0f);
    rng_vec(&ob, &pb, -10.0f, 10.0f);
    oracle_C_VECCrossProduct(&oa, &ob, &ov);
    C_VECCrossProduct(&pa, &pb, &pv);
    CHECK_VEC("VECCrossProduct", ov, pv);

    /* VECNormalize */
    rng_vec_nonzero(&oa, &pa);
    oracle_C_VECNormalize(&oa, &ov);
    C_VECNormalize(&pa, &pv);
    CHECK_VEC("VECNormalize", ov, pv);

    /* VECSquareMag */
    rng_vec(&oa, &pa, -10.0f, 10.0f);
    CHECK_FLOAT("VECSquareMag", oracle_C_VECSquareMag(&oa), C_VECSquareMag(&pa));

    /* VECMag */
    rng_vec(&oa, &pa, -10.0f, 10.0f);
    CHECK_FLOAT("VECMag", oracle_C_VECMag(&oa), C_VECMag(&pa));

    /* VECSquareDistance */
    rng_vec(&oa, &pa, -10.0f, 10.0f);
    rng_vec(&ob, &pb, -10.0f, 10.0f);
    CHECK_FLOAT("VECSquareDistance", oracle_C_VECSquareDistance(&oa, &ob), C_VECSquareDistance(&pa, &pb));

    /* VECDistance */
    rng_vec(&oa, &pa, -10.0f, 10.0f);
    rng_vec(&ob, &pb, -10.0f, 10.0f);
    CHECK_FLOAT("VECDistance", oracle_C_VECDistance(&oa, &ob), C_VECDistance(&pa, &pb));

    /* VECHalfAngle */
    rng_vec_nonzero(&oa, &pa);
    rng_vec_nonzero(&ob, &pb);
    oracle_C_VECHalfAngle(&oa, &ob, &ov);
    C_VECHalfAngle(&pa, &pb, &pv);
    CHECK_VEC("VECHalfAngle", ov, pv);

    /* VECReflect */
    rng_vec_nonzero(&oa, &pa);
    rng_vec_nonzero(&ob, &pb);
    oracle_C_VECReflect(&oa, &ob, &ov);
    C_VECReflect(&pa, &pb, &pv);
    CHECK_VEC("VECReflect", ov, pv);
}

static void test_quat_api(unsigned int seed)
{
    oracle_Quaternion oq1, oq2, oqr; Quaternion pq1, pq2, pqr;

    /* QUATAdd */
    rng_quat(&oq1, &pq1);
    rng_quat(&oq2, &pq2);
    oracle_C_QUATAdd(&oq1, &oq2, &oqr);
    C_QUATAdd(&pq1, &pq2, &pqr);
    CHECK_QUAT("QUATAdd", oqr, pqr);

    /* QUATRotAxisRad */
    oracle_Vec oaxis; Vec paxis;
    rng_vec_nonzero(&oaxis, &paxis);
    float rad = rng_float(-3.14159f, 3.14159f);
    oracle_C_QUATRotAxisRad(&oqr, &oaxis, rad);
    C_QUATRotAxisRad(&pqr, &paxis, rad);
    CHECK_QUAT("QUATRotAxisRad", oqr, pqr);

    /* QUATMtx */
    oracle_Mtx om; Mtx pm;
    rng_quat(&oq1, &pq1);
    oracle_C_MTXQuat(om, &oq1);
    C_MTXQuat(pm, &pq1);
    oracle_C_QUATMtx(&oqr, om);
    C_QUATMtx(&pqr, pm);
    CHECK_QUAT("QUATMtx", oqr, pqr);

    /* QUATSlerp */
    rng_quat(&oq1, &pq1);
    rng_quat(&oq2, &pq2);
    float t = rng_float(0.0f, 1.0f);
    oracle_C_QUATSlerp(&oq1, &oq2, &oqr, t);
    C_QUATSlerp(&pq1, &pq2, &pqr, t);
    CHECK_QUAT("QUATSlerp", oqr, pqr);
}

static void test_mtx_api(unsigned int seed)
{
    oracle_Mtx om, om2, omr; Mtx pm, pm2, pmr;

    /* MTXIdentity — decomp doesn't set column 3, so zero first */
    memset(om, 0, sizeof(om)); memset(pm, 0, sizeof(pm));
    oracle_C_MTXIdentity(om);
    C_MTXIdentity(pm);
    CHECK_MTX("MTXIdentity", om, pm);

    /* MTXCopy */
    rng_mtx(om, pm);
    oracle_C_MTXCopy(om, omr);
    C_MTXCopy(pm, pmr);
    CHECK_MTX("MTXCopy", omr, pmr);

    /* MTXConcat */
    rng_mtx(om, pm);
    rng_mtx(om2, pm2);
    oracle_C_MTXConcat(om, om2, omr);
    C_MTXConcat(pm, pm2, pmr);
    CHECK_MTX("MTXConcat", omr, pmr);

    /* MTXTranspose */
    rng_mtx(om, pm);
    oracle_C_MTXTranspose(om, omr);
    C_MTXTranspose(pm, pmr);
    CHECK_MTX("MTXTranspose", omr, pmr);

    /* MTXInverse */
    rng_mtx(om, pm);
    oracle_u32 oi = oracle_C_MTXInverse(om, omr);
    u32 pi = C_MTXInverse(pm, pmr);
    CHECK_U32("MTXInverse_ret", oi, pi);
    if (oi && pi) CHECK_MTX("MTXInverse", omr, pmr);

    /* MTXInvXpose */
    rng_mtx(om, pm);
    oi = oracle_C_MTXInvXpose(om, omr);
    pi = C_MTXInvXpose(pm, pmr);
    CHECK_U32("MTXInvXpose_ret", oi, pi);
    if (oi && pi) CHECK_MTX("MTXInvXpose", omr, pmr);

    /* MTXRotRad (all axes) */
    {
        char axes[] = "xyzXYZ";
        for (int i = 0; axes[i]; i++) {
            float rad = rng_float(-6.28f, 6.28f);
            oracle_C_MTXRotRad(om, axes[i], rad);
            C_MTXRotRad(pm, axes[i], rad);
            char name[32]; snprintf(name, sizeof(name), "MTXRotRad_%c", axes[i]);
            CHECK_MTX(name, om, pm);
        }
    }

    /* MTXRotTrig */
    {
        float sinA = rng_float(-1.0f, 1.0f);
        float cosA = rng_float(-1.0f, 1.0f);
        oracle_C_MTXRotTrig(om, 'x', sinA, cosA);
        C_MTXRotTrig(pm, 'x', sinA, cosA);
        CHECK_MTX("MTXRotTrig", om, pm);
    }

    /* MTXRotAxisRad */
    {
        oracle_Vec oaxis; Vec paxis;
        rng_vec_nonzero(&oaxis, &paxis);
        float rad = rng_float(-6.28f, 6.28f);
        oracle_C_MTXRotAxisRad(om, &oaxis, rad);
        C_MTXRotAxisRad(pm, &paxis, rad);
        CHECK_MTX("MTXRotAxisRad", om, pm);
    }

    /* MTXTrans */
    {
        float x = rng_float(-100.0f, 100.0f);
        float y = rng_float(-100.0f, 100.0f);
        float z = rng_float(-100.0f, 100.0f);
        oracle_C_MTXTrans(om, x, y, z);
        C_MTXTrans(pm, x, y, z);
        CHECK_MTX("MTXTrans", om, pm);
    }

    /* MTXTransApply */
    {
        rng_mtx(om, pm);
        float x = rng_float(-100.0f, 100.0f);
        float y = rng_float(-100.0f, 100.0f);
        float z = rng_float(-100.0f, 100.0f);
        oracle_C_MTXTransApply(om, omr, x, y, z);
        C_MTXTransApply(pm, pmr, x, y, z);
        CHECK_MTX("MTXTransApply", omr, pmr);
    }

    /* MTXScale */
    {
        float x = rng_float(-10.0f, 10.0f);
        float y = rng_float(-10.0f, 10.0f);
        float z = rng_float(-10.0f, 10.0f);
        oracle_C_MTXScale(om, x, y, z);
        C_MTXScale(pm, x, y, z);
        CHECK_MTX("MTXScale", om, pm);
    }

    /* MTXScaleApply */
    {
        rng_mtx(om, pm);
        float x = rng_float(-10.0f, 10.0f);
        float y = rng_float(-10.0f, 10.0f);
        float z = rng_float(-10.0f, 10.0f);
        oracle_C_MTXScaleApply(om, omr, x, y, z);
        C_MTXScaleApply(pm, pmr, x, y, z);
        CHECK_MTX("MTXScaleApply", omr, pmr);
    }

    /* MTXQuat */
    {
        oracle_Quaternion oq; Quaternion pq;
        rng_quat(&oq, &pq);
        oracle_C_MTXQuat(om, &oq);
        C_MTXQuat(pm, &pq);
        CHECK_MTX("MTXQuat", om, pm);
    }

    /* MTXReflect */
    {
        oracle_Vec op, on; Vec pp, pn;
        rng_vec(&op, &pp, -10.0f, 10.0f);
        rng_vec_nonzero(&on, &pn);
        oracle_C_MTXReflect(om, &op, &on);
        C_MTXReflect(pm, &pp, &pn);
        CHECK_MTX("MTXReflect", om, pm);
    }

    /* MTXLookAt */
    {
        oracle_Vec ocam, oup, otgt; Vec pcam, pup, ptgt;
        rng_vec(&ocam, &pcam, -10.0f, 10.0f);
        rng_vec_nonzero(&oup, &pup);
        rng_vec(&otgt, &ptgt, -10.0f, 10.0f);
        /* Ensure cam != target */
        otgt.x += 1.0f; ptgt.x += 1.0f;
        oracle_C_MTXLookAt(om, &ocam, &oup, &otgt);
        C_MTXLookAt(pm, &pcam, &pup, &ptgt);
        CHECK_MTX("MTXLookAt", om, pm);
    }

    /* MTXMultVec */
    {
        oracle_Vec ov, ovr; Vec pv, pvr;
        rng_mtx(om, pm);
        rng_vec(&ov, &pv, -10.0f, 10.0f);
        oracle_C_MTXMultVec(om, &ov, &ovr);
        C_MTXMultVec(pm, &pv, &pvr);
        CHECK_VEC("MTXMultVec", ovr, pvr);
    }

    /* MTXMultVecSR */
    {
        oracle_Vec ov, ovr; Vec pv, pvr;
        rng_mtx(om, pm);
        rng_vec(&ov, &pv, -10.0f, 10.0f);
        oracle_C_MTXMultVecSR(om, &ov, &ovr);
        C_MTXMultVecSR(pm, &pv, &pvr);
        CHECK_VEC("MTXMultVecSR", ovr, pvr);
    }

    /* MTXLightFrustum */
    {
        float t = rng_float(0.5f, 5.0f), b = -t;
        float r = rng_float(0.5f, 5.0f), l = -r;
        float n = rng_float(0.1f, 1.0f);
        float sS = rng_float(0.1f, 2.0f), sT = rng_float(0.1f, 2.0f);
        float tS = rng_float(-1.0f, 1.0f), tT = rng_float(-1.0f, 1.0f);
        oracle_C_MTXLightFrustum(om, t, b, l, r, n, sS, sT, tS, tT);
        C_MTXLightFrustum(pm, t, b, l, r, n, sS, sT, tS, tT);
        CHECK_MTX("MTXLightFrustum", om, pm);
    }

    /* MTXLightPerspective */
    {
        float fov = rng_float(30.0f, 120.0f);
        float aspect = rng_float(0.5f, 2.0f);
        float sS = rng_float(0.1f, 2.0f), sT = rng_float(0.1f, 2.0f);
        float tS = rng_float(-1.0f, 1.0f), tT = rng_float(-1.0f, 1.0f);
        oracle_C_MTXLightPerspective(om, fov, aspect, sS, sT, tS, tT);
        C_MTXLightPerspective(pm, fov, aspect, sS, sT, tS, tT);
        CHECK_MTX("MTXLightPerspective", om, pm);
    }

    /* MTXLightOrtho */
    {
        float t = rng_float(0.5f, 5.0f), b = -t;
        float r = rng_float(0.5f, 5.0f), l = -r;
        float sS = rng_float(0.1f, 2.0f), sT = rng_float(0.1f, 2.0f);
        float tS = rng_float(-1.0f, 1.0f), tT = rng_float(-1.0f, 1.0f);
        oracle_C_MTXLightOrtho(om, t, b, l, r, sS, sT, tS, tT);
        C_MTXLightOrtho(pm, t, b, l, r, sS, sT, tS, tT);
        CHECK_MTX("MTXLightOrtho", om, pm);
    }
}

static void test_mtx44_api(unsigned int seed)
{
    oracle_Mtx44 om44; Mtx44 pm44;

    /* MTXFrustum */
    {
        float t = rng_float(0.5f, 5.0f), b = -t;
        float r = rng_float(0.5f, 5.0f), l = -r;
        float n = rng_float(0.1f, 1.0f), f = rng_float(10.0f, 1000.0f);
        oracle_C_MTXFrustum(om44, t, b, l, r, n, f);
        C_MTXFrustum(pm44, t, b, l, r, n, f);
        CHECK_MTX44("MTXFrustum", om44, pm44);
    }

    /* MTXPerspective */
    {
        float fov = rng_float(30.0f, 120.0f);
        float aspect = rng_float(0.5f, 2.0f);
        float n = rng_float(0.1f, 1.0f), f = rng_float(10.0f, 1000.0f);
        oracle_C_MTXPerspective(om44, fov, aspect, n, f);
        C_MTXPerspective(pm44, fov, aspect, n, f);
        CHECK_MTX44("MTXPerspective", om44, pm44);
    }

    /* MTXOrtho */
    {
        float t = rng_float(0.5f, 5.0f), b = -t;
        float r = rng_float(0.5f, 5.0f), l = -r;
        float n = rng_float(0.1f, 1.0f), f = rng_float(10.0f, 1000.0f);
        oracle_C_MTXOrtho(om44, t, b, l, r, n, f);
        C_MTXOrtho(pm44, t, b, l, r, n, f);
        CHECK_MTX44("MTXOrtho", om44, pm44);
    }
}

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(int argc, char **argv)
{
    int num_runs = 500;
    unsigned int seed_override = 0;
    int single_seed = 0;
    const char *op_filter = NULL;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            seed_override = (unsigned int)atoi(argv[i] + 7);
            single_seed = 1;
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            num_runs = atoi(argv[i] + 11);
        } else if (strncmp(argv[i], "--op=", 5) == 0) {
            op_filter = argv[i] + 5;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        }
    }

    int run_vec  = !op_filter || strstr(op_filter, "VEC");
    int run_quat = !op_filter || strstr(op_filter, "QUAT");
    int run_mtx  = !op_filter || strstr(op_filter, "MTX") || strstr(op_filter, "Mtx");
    int run_m44  = !op_filter || strstr(op_filter, "44");

    unsigned int start = single_seed ? seed_override : 1;
    unsigned int end   = single_seed ? seed_override : (unsigned int)num_runs;

    printf("[mtx-property] testing seeds %u..%u", start, end);
    if (op_filter) printf("  op=%s", op_filter);
    printf("\n");

    for (unsigned int seed = start; seed <= end; seed++) {
        rng_seed(seed);

        if (run_vec)  test_vec_leaves(seed);
        if (run_quat) test_quat_api(seed);
        if (run_mtx)  test_mtx_api(seed);
        if (run_m44)  test_mtx44_api(seed);
    }

    printf("[mtx-property] %d pass, %d fail\n", total_pass, total_fail);
    return total_fail > 0 ? 1 : 0;
}
