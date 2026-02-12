/*
 * gxlight_property_test.c --- Property-based parity test for GXLight functions.
 *
 * Oracle:  exact decomp copy (gxlight_oracle.h)
 * Port:    sdk_port/gx/GX.c
 *
 * Test levels:
 *   L0 — GXInitLightSpot: oracle vs port parity for all 7 spot functions
 *   L1 — GXInitLightDistAttn: oracle vs port parity for all 4 dist functions
 *   L2 — GXInitSpecularDir: oracle vs port parity + lpos scaling
 *   L3 — GXInitLightAttn: oracle vs port parity (simple assignment)
 *   L4 — GXInitLightColor: oracle vs port parity (RGBA packing)
 *   L5 — Edge cases: boundary cutoff, degenerate ref_br, zero vectors
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* Oracle */
#include "gxlight_oracle.h"

/* Port function declarations (linked from GX.c).
 * GXLightObj in GX.c has the same memory layout as oracle_GXLightObj.
 * Enums share the same integer values (0..6 for spot, 0..3 for dist). */
typedef oracle_GXLightObj port_GXLightObj;

extern void GXInitLightAttn(port_GXLightObj *lt, float a0, float a1, float a2,
                             float k0, float k1, float k2);
extern void GXInitLightAttnK(port_GXLightObj *lt, float k0, float k1, float k2);
extern void GXInitLightSpot(port_GXLightObj *lt, float cutoff, int spot_func);
extern void GXInitLightDistAttn(port_GXLightObj *lt, float ref_dist, float ref_br,
                                 int dist_func);
extern void GXInitSpecularDir(port_GXLightObj *lt, float nx, float ny, float nz);
extern void GXInitLightColor(port_GXLightObj *lt, oracle_GXColor color);

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

static int light_eq(const oracle_GXLightObj *a, const oracle_GXLightObj *b) {
    return memcmp(a, b, sizeof(oracle_GXLightObj)) == 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * L0: GXInitLightSpot — all 7 spot functions, random cutoff
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L0_spot(void)
{
    int fn;
    for (fn = 0; fn <= 6; fn++) {
        float cutoff = rand_float(0.1f, 89.9f);
        oracle_GXLightObj o = {0}, p = {0};

        oracle_GXInitLightSpot(&o, cutoff, (oracle_GXSpotFn)fn);
        GXInitLightSpot(&p, cutoff, fn);

        CHECK(o.a[0] == p.a[0] && o.a[1] == p.a[1] && o.a[2] == p.a[2],
              "L0 spot fn=%d cutoff=%.4f: o=[%.6f,%.6f,%.6f] p=[%.6f,%.6f,%.6f]",
              fn, cutoff, o.a[0], o.a[1], o.a[2], p.a[0], p.a[1], p.a[2]);
    }

    /* Invalid cutoff => SP_OFF */
    {
        oracle_GXLightObj o = {0}, p = {0};
        oracle_GXInitLightSpot(&o, -1.0f, ORACLE_GX_SP_COS);
        GXInitLightSpot(&p, -1.0f, 2); /* GX_SP_COS=2 */
        CHECK(o.a[0] == p.a[0] && o.a[1] == p.a[1] && o.a[2] == p.a[2],
              "L0 spot negative cutoff");
    }
    {
        oracle_GXLightObj o = {0}, p = {0};
        oracle_GXInitLightSpot(&o, 91.0f, ORACLE_GX_SP_FLAT);
        GXInitLightSpot(&p, 91.0f, 1);
        CHECK(o.a[0] == p.a[0] && o.a[1] == p.a[1] && o.a[2] == p.a[2],
              "L0 spot cutoff>90");
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L1: GXInitLightDistAttn — all 4 dist functions
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L1_dist(void)
{
    int fn;
    for (fn = 0; fn <= 3; fn++) {
        float ref_dist = rand_float(1.0f, 1000.0f);
        float ref_br = rand_float(0.01f, 0.99f);
        oracle_GXLightObj o = {0}, p = {0};

        oracle_GXInitLightDistAttn(&o, ref_dist, ref_br, (oracle_GXDistAttnFn)fn);
        GXInitLightDistAttn(&p, ref_dist, ref_br, fn);

        CHECK(o.k[0] == p.k[0] && o.k[1] == p.k[1] && o.k[2] == p.k[2],
              "L1 dist fn=%d dist=%.2f br=%.4f: o=[%.6f,%.6f,%.6f] p=[%.6f,%.6f,%.6f]",
              fn, ref_dist, ref_br, o.k[0], o.k[1], o.k[2], p.k[0], p.k[1], p.k[2]);
    }

    /* Invalid ref_br => DA_OFF */
    {
        oracle_GXLightObj o = {0}, p = {0};
        oracle_GXInitLightDistAttn(&o, 100.0f, 0.0f, ORACLE_GX_DA_GENTLE);
        GXInitLightDistAttn(&p, 100.0f, 0.0f, 1);
        CHECK(o.k[0] == p.k[0] && o.k[1] == p.k[1] && o.k[2] == p.k[2],
              "L1 dist ref_br=0");
    }
    {
        oracle_GXLightObj o = {0}, p = {0};
        oracle_GXInitLightDistAttn(&o, 100.0f, 1.0f, ORACLE_GX_DA_STEEP);
        GXInitLightDistAttn(&p, 100.0f, 1.0f, 3);
        CHECK(o.k[0] == p.k[0] && o.k[1] == p.k[1] && o.k[2] == p.k[2],
              "L1 dist ref_br=1");
    }
    /* Negative ref_dist => DA_OFF */
    {
        oracle_GXLightObj o = {0}, p = {0};
        oracle_GXInitLightDistAttn(&o, -5.0f, 0.5f, ORACLE_GX_DA_MEDIUM);
        GXInitLightDistAttn(&p, -5.0f, 0.5f, 2);
        CHECK(o.k[0] == p.k[0] && o.k[1] == p.k[1] && o.k[2] == p.k[2],
              "L1 dist negative ref_dist");
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L2: GXInitSpecularDir — normal vector => half-angle + scaled pos
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L2_specular(void)
{
    float nx = rand_float(-1.0f, 1.0f);
    float ny = rand_float(-1.0f, 1.0f);
    float nz = rand_float(-1.0f, 1.0f);
    oracle_GXLightObj o = {0}, p = {0};

    oracle_GXInitSpecularDir(&o, nx, ny, nz);
    GXInitSpecularDir(&p, nx, ny, nz);

    CHECK(o.ldir[0] == p.ldir[0] && o.ldir[1] == p.ldir[1] && o.ldir[2] == p.ldir[2],
          "L2 specular ldir: n=[%.4f,%.4f,%.4f]", nx, ny, nz);
    CHECK(o.lpos[0] == p.lpos[0] && o.lpos[1] == p.lpos[1] && o.lpos[2] == p.lpos[2],
          "L2 specular lpos: n=[%.4f,%.4f,%.4f]", nx, ny, nz);

    /* lpos should be -n * 1048576 */
    CHECK(o.lpos[0] == -nx * 1048576.0f, "L2 specular lpos[0] = -nx*1048576");
    CHECK(o.lpos[1] == -ny * 1048576.0f, "L2 specular lpos[1] = -ny*1048576");
    CHECK(o.lpos[2] == -nz * 1048576.0f, "L2 specular lpos[2] = -nz*1048576");

    /* ldir should be normalized half-angle */
    float mag = o.ldir[0]*o.ldir[0] + o.ldir[1]*o.ldir[1] + o.ldir[2]*o.ldir[2];
    float diff = mag - 1.0f;
    if (diff < 0) diff = -diff;
    CHECK(diff < 1e-4f, "L2 specular ldir unit length: |ldir|^2=%.6f", mag);
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: GXInitLightAttn — assignment parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L3_attn(void)
{
    float a0 = rand_float(-10.0f, 10.0f);
    float a1 = rand_float(-10.0f, 10.0f);
    float a2 = rand_float(-10.0f, 10.0f);
    float k0 = rand_float(-10.0f, 10.0f);
    float k1 = rand_float(-10.0f, 10.0f);
    float k2 = rand_float(-10.0f, 10.0f);
    oracle_GXLightObj o = {0}, p = {0};

    oracle_GXInitLightAttn(&o, a0, a1, a2, k0, k1, k2);
    GXInitLightAttn(&p, a0, a1, a2, k0, k1, k2);

    CHECK(o.a[0]==p.a[0] && o.a[1]==p.a[1] && o.a[2]==p.a[2],
          "L3 attn a[] parity");
    CHECK(o.k[0]==p.k[0] && o.k[1]==p.k[1] && o.k[2]==p.k[2],
          "L3 attn k[] parity");

    /* GXInitLightAttnK */
    oracle_GXInitLightAttnK(&o, a2, a0, a1);
    GXInitLightAttnK(&p, a2, a0, a1);
    CHECK(o.k[0]==p.k[0] && o.k[1]==p.k[1] && o.k[2]==p.k[2],
          "L3 attnK parity");
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: GXInitLightColor — RGBA packing
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L4_color(void)
{
    oracle_GXColor c;
    c.r = (u8)(xorshift32() & 0xFF);
    c.g = (u8)(xorshift32() & 0xFF);
    c.b = (u8)(xorshift32() & 0xFF);
    c.a = (u8)(xorshift32() & 0xFF);
    oracle_GXLightObj o = {0}, p = {0};

    oracle_GXInitLightColor(&o, c);
    GXInitLightColor(&p, c);

    CHECK(o.Color == p.Color,
          "L4 color r=%u g=%u b=%u a=%u: oracle=0x%08X port=0x%08X",
          c.r, c.g, c.b, c.a, o.Color, p.Color);
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Edge cases
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L5_edge(void)
{
    oracle_GXLightObj o = {0}, p = {0};

    /* Spot at exact boundary: cutoff=90 is valid */
    oracle_GXInitLightSpot(&o, 90.0f, ORACLE_GX_SP_COS);
    GXInitLightSpot(&p, 90.0f, 2);
    CHECK(o.a[0]==p.a[0] && o.a[1]==p.a[1] && o.a[2]==p.a[2],
          "L5 spot cutoff=90");

    /* Specular with zero vector */
    memset(&o, 0, sizeof(o));
    memset(&p, 0, sizeof(p));
    oracle_GXInitSpecularDir(&o, 0.0f, 0.0f, 0.0f);
    GXInitSpecularDir(&p, 0.0f, 0.0f, 0.0f);
    CHECK(o.ldir[0]==p.ldir[0] && o.ldir[1]==p.ldir[1] && o.ldir[2]==p.ldir[2],
          "L5 specular zero ldir");
    CHECK(o.lpos[0]==p.lpos[0] && o.lpos[1]==p.lpos[1] && o.lpos[2]==p.lpos[2],
          "L5 specular zero lpos");

    /* Dist attn with very small ref_br */
    memset(&o, 0, sizeof(o));
    memset(&p, 0, sizeof(p));
    oracle_GXInitLightDistAttn(&o, 100.0f, 0.001f, ORACLE_GX_DA_STEEP);
    GXInitLightDistAttn(&p, 100.0f, 0.001f, 3);
    CHECK(o.k[0]==p.k[0] && o.k[1]==p.k[1] && o.k[2]==p.k[2],
          "L5 dist tiny ref_br");
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

    printf("gxlight_property_test: seed=%u num_runs=%d\n", seed, num_runs);

    for (i = 0; i < num_runs; i++) {
        g_rng = seed + (uint32_t)i;

        test_L0_spot();
        test_L1_dist();
        test_L2_specular();
        test_L3_attn();
        test_L4_color();
        test_L5_edge();
    }

    printf("\ngxlight_property_test: %d/%d PASS", g_pass, g_pass + g_fail);
    if (g_fail) printf(", %d FAIL", g_fail);
    printf("\n");

    return g_fail ? 1 : 0;
}
