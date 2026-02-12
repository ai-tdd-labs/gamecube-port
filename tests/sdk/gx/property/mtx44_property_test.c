/*
 * mtx44_property_test.c --- Property-based parity test for MTX44 functions.
 *
 * Oracle:  exact decomp copy (mtx44_oracle.h)
 * Port:    sdk_port/mtx/mtx44.c
 *
 * Test levels:
 *   L0 — C_MTXFrustum: oracle vs port parity
 *   L1 — C_MTXPerspective: oracle vs port parity
 *   L2 — C_MTXOrtho: oracle vs port parity
 *   L3 — Structural checks: zero elements, sign of perspective row[3]
 *   L4 — Ortho identity: symmetric bounds produce identity-like scaling
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* Oracle */
#include "mtx44_oracle.h"

/* Port (linked from sdk_port/mtx/mtx44.c) */
typedef f32 port_Mtx44[4][4];
extern void C_MTXFrustum(port_Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
extern void C_MTXPerspective(port_Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f);
extern void C_MTXOrtho(port_Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);

/* ── xorshift32 PRNG ── */
static uint32_t g_rng;
static uint32_t xorshift32(void) {
    uint32_t x = g_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g_rng = x;
    return x;
}
static float rand_float(float lo, float hi) {
    uint32_t r = xorshift32();
    float t = (float)(r & 0xFFFFFF) / (float)0xFFFFFF;
    return lo + t * (hi - lo);
}

/* ── Test infrastructure ── */
static int g_verbose;
static int g_pass, g_fail;

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); \
        g_fail++; return; \
    } else { g_pass++; } \
} while (0)

static int mtx44_eq(const f32 a[4][4], const f32 b[4][4]) {
    return memcmp(a, b, 16 * sizeof(f32)) == 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * L0: C_MTXFrustum parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L0_frustum(void)
{
    float l = rand_float(-10.0f, -0.1f);
    float r = rand_float(0.1f, 10.0f);
    float b = rand_float(-10.0f, -0.1f);
    float t = rand_float(0.1f, 10.0f);
    float n = rand_float(0.1f, 10.0f);
    float f = rand_float(n + 0.1f, 1000.0f);

    oracle_Mtx44 o;
    port_Mtx44 p;

    oracle_C_MTXFrustum(o, t, b, l, r, n, f);
    C_MTXFrustum(p, t, b, l, r, n, f);

    CHECK(mtx44_eq(o, p),
          "L0 frustum parity: l=%.2f r=%.2f b=%.2f t=%.2f n=%.2f f=%.2f",
          l, r, b, t, n, f);
}

/* ═══════════════════════════════════════════════════════════════════
 * L1: C_MTXPerspective parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L1_perspective(void)
{
    float fovY = rand_float(10.0f, 170.0f);
    float aspect = rand_float(0.5f, 3.0f);
    float n = rand_float(0.01f, 10.0f);
    float f = rand_float(n + 1.0f, 10000.0f);

    oracle_Mtx44 o;
    port_Mtx44 p;

    oracle_C_MTXPerspective(o, fovY, aspect, n, f);
    C_MTXPerspective(p, fovY, aspect, n, f);

    CHECK(mtx44_eq(o, p),
          "L1 perspective parity: fov=%.2f aspect=%.2f n=%.2f f=%.2f",
          fovY, aspect, n, f);
}

/* ═══════════════════════════════════════════════════════════════════
 * L2: C_MTXOrtho parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L2_ortho(void)
{
    float l = rand_float(-100.0f, -0.1f);
    float r = rand_float(0.1f, 100.0f);
    float b = rand_float(-100.0f, -0.1f);
    float t = rand_float(0.1f, 100.0f);
    float n = rand_float(0.1f, 10.0f);
    float f = rand_float(n + 0.1f, 1000.0f);

    oracle_Mtx44 o;
    port_Mtx44 p;

    oracle_C_MTXOrtho(o, t, b, l, r, n, f);
    C_MTXOrtho(p, t, b, l, r, n, f);

    CHECK(mtx44_eq(o, p),
          "L2 ortho parity: l=%.2f r=%.2f b=%.2f t=%.2f n=%.2f f=%.2f",
          l, r, b, t, n, f);
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: Structural checks
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L3_structure(void)
{
    float n = rand_float(0.1f, 10.0f);
    float f = rand_float(n + 1.0f, 1000.0f);
    float fovY = rand_float(10.0f, 170.0f);
    float aspect = rand_float(0.5f, 3.0f);
    oracle_Mtx44 m;

    /* Perspective: row[3] = [0, 0, -1, 0] */
    oracle_C_MTXPerspective(m, fovY, aspect, n, f);
    CHECK(m[3][0] == 0.0f && m[3][1] == 0.0f && m[3][2] == -1.0f && m[3][3] == 0.0f,
          "L3 perspective row3=[0,0,-1,0]");

    /* Perspective: m[0][0] > 0 (positive X scale) */
    CHECK(m[0][0] > 0.0f, "L3 perspective m[0][0] > 0");
    CHECK(m[1][1] > 0.0f, "L3 perspective m[1][1] > 0");

    /* Ortho: row[3] = [0, 0, 0, 1] */
    float l = -10.0f, r = 10.0f, b = -10.0f, t = 10.0f;
    oracle_C_MTXOrtho(m, t, b, l, r, n, f);
    CHECK(m[3][0] == 0.0f && m[3][1] == 0.0f && m[3][2] == 0.0f && m[3][3] == 1.0f,
          "L3 ortho row3=[0,0,0,1]");
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: Ortho symmetric bounds → centered output
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L4_ortho_symmetric(void)
{
    float half = rand_float(1.0f, 100.0f);
    float n = 0.1f, f = 100.0f;
    oracle_Mtx44 m;

    oracle_C_MTXOrtho(m, half, -half, -half, half, n, f);

    /* Symmetric: translation terms m[0][3] and m[1][3] should be 0 */
    CHECK(m[0][3] == 0.0f, "L4 ortho symmetric m[0][3]=0");
    CHECK(m[1][3] == 0.0f, "L4 ortho symmetric m[1][3]=0");

    /* Scaling: m[0][0] = 2/(2*half) = 1/half */
    float expected = 1.0f / half;
    float diff = m[0][0] - expected;
    if (diff < 0) diff = -diff;
    CHECK(diff < 1e-5f, "L4 ortho symmetric m[0][0]=1/half");
}

/* ═══════════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════════ */
int main(int argc, char **argv)
{
    int num_runs = 2000;
    uint32_t seed = 12345;
    int i;
    g_verbose = 0;

    for (i = 1; i < argc; i++) {
        if (sscanf(argv[i], "--seed=%u", &seed) == 1) continue;
        if (sscanf(argv[i], "--num-runs=%d", &num_runs) == 1) continue;
        if (strcmp(argv[i], "-v") == 0) { g_verbose = 1; continue; }
    }

    printf("mtx44_property_test: seed=%u num_runs=%d\n", seed, num_runs);

    for (i = 0; i < num_runs; i++) {
        g_rng = seed + (uint32_t)i;

        test_L0_frustum();
        test_L1_perspective();
        test_L2_ortho();
        test_L3_structure();
        test_L4_ortho_symmetric();
    }

    printf("\nmtx44_property_test: %d/%d PASS", g_pass, g_pass + g_fail);
    if (g_fail) printf(", %d FAIL", g_fail);
    printf("\n");

    return g_fail ? 1 : 0;
}
