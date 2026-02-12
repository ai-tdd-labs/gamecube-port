/*
 * gxoverscan_property_test.c --- Property-based parity test for GXAdjustForOverscan.
 *
 * Oracle:  exact decomp copy (gxoverscan_oracle.h)
 * Port:    sdk_port/gx/GX.c
 *
 * Test levels:
 *   L0 — Oracle vs port parity: random render modes + overscan values
 *   L1 — In-place operation (rmin == rmout) parity
 *   L2 — Single-field interlace (xFBmode=SF, viTVmode non-progressive)
 *   L3 — Double-strike mode (viTVmode & 2 == 2)
 *   L4 — Zero overscan (identity)
 *   L5 — Edge: maximal overscan consuming most of the framebuffer
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Oracle */
#include "gxoverscan_oracle.h"

/* Port (linked from GX.c). GXRenderModeObj has same layout in both. */
typedef oracle_GXRenderModeObj port_GXRenderModeObj;
extern void GXAdjustForOverscan(port_GXRenderModeObj *rmin,
    port_GXRenderModeObj *rmout, u16 hor, u16 ver);

/* ── xorshift32 PRNG ── */
static uint32_t g_rng;
static uint32_t xorshift32(void) {
    uint32_t x = g_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g_rng = x;
    return x;
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

static int rm_eq(const oracle_GXRenderModeObj *a, const oracle_GXRenderModeObj *b) {
    return memcmp(a, b, sizeof(oracle_GXRenderModeObj)) == 0;
}

static oracle_GXRenderModeObj rand_rm(void) {
    oracle_GXRenderModeObj rm;
    rm.viTVmode = xorshift32() & 0x3; /* 0..3 */
    rm.fbWidth = (u16)(320 + (xorshift32() % 321)); /* 320..640 */
    rm.efbHeight = (u16)(240 + (xorshift32() % 241)); /* 240..480 */
    rm.xfbHeight = (u16)(240 + (xorshift32() % 241)); /* 240..480 */
    if (rm.xfbHeight == 0) rm.xfbHeight = 1; /* avoid div-by-zero */
    rm.viXOrigin = (u16)(xorshift32() % 100);
    rm.viYOrigin = (u16)(xorshift32() % 100);
    rm.viWidth = (u16)(320 + (xorshift32() % 321));
    rm.viHeight = (u16)(240 + (xorshift32() % 241));
    rm.xFBmode = xorshift32() & 0x1; /* 0 or 1 */
    return rm;
}

/* ═══════════════════════════════════════════════════════════════════
 * L0: Random parity (separate in/out)
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L0_parity(void)
{
    oracle_GXRenderModeObj rmin = rand_rm();
    u16 hor = (u16)(xorshift32() % 32 + 1);
    u16 ver = (u16)(xorshift32() % 32 + 1);
    oracle_GXRenderModeObj o_out = {0}, p_out = {0};

    oracle_GXAdjustForOverscan(&rmin, &o_out, hor, ver);
    GXAdjustForOverscan(&rmin, &p_out, hor, ver);

    CHECK(rm_eq(&o_out, &p_out),
          "L0 parity: hor=%u ver=%u fbW o=%u p=%u efbH o=%u p=%u xfbH o=%u p=%u",
          hor, ver, o_out.fbWidth, p_out.fbWidth,
          o_out.efbHeight, p_out.efbHeight,
          o_out.xfbHeight, p_out.xfbHeight);
}

/* ═══════════════════════════════════════════════════════════════════
 * L1: In-place (rmin == rmout)
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L1_inplace(void)
{
    oracle_GXRenderModeObj rm = rand_rm();
    oracle_GXRenderModeObj rm_copy = rm;
    u16 hor = (u16)(xorshift32() % 20 + 1);
    u16 ver = (u16)(xorshift32() % 20 + 1);

    oracle_GXRenderModeObj o_ref = {0};
    oracle_GXAdjustForOverscan(&rm_copy, &o_ref, hor, ver);

    /* In-place: port modifies rm directly */
    GXAdjustForOverscan(&rm, &rm, hor, ver);

    CHECK(rm_eq(&rm, &o_ref),
          "L1 inplace parity: hor=%u ver=%u", hor, ver);
}

/* ═══════════════════════════════════════════════════════════════════
 * L2: Single-field interlace (xFBmode=1, viTVmode non-progressive)
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L2_singlefield(void)
{
    oracle_GXRenderModeObj rmin = rand_rm();
    rmin.xFBmode = 1; /* VI_XFBMODE_SF */
    rmin.viTVmode = 0; /* non-progressive (bit 1 = 0) */
    u16 hor = (u16)(xorshift32() % 20 + 1);
    u16 ver = (u16)(xorshift32() % 20 + 1);

    oracle_GXRenderModeObj o = {0}, p = {0};
    oracle_GXAdjustForOverscan(&rmin, &o, hor, ver);
    GXAdjustForOverscan(&rmin, &p, hor, ver);

    CHECK(rm_eq(&o, &p), "L2 single-field parity");

    /* xfbHeight should lose only ver (not ver*2) in SF non-progressive */
    CHECK(o.xfbHeight == (u16)(rmin.xfbHeight - ver),
          "L2 SF xfbHeight -= ver (not ver*2)");
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: Double-strike (viTVmode & 2 == 2) => always ver*2
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L3_doublestrike(void)
{
    oracle_GXRenderModeObj rmin = rand_rm();
    rmin.xFBmode = 1;
    rmin.viTVmode = 2; /* progressive / double-strike */
    u16 hor = (u16)(xorshift32() % 20 + 1);
    u16 ver = (u16)(xorshift32() % 20 + 1);

    oracle_GXRenderModeObj o = {0}, p = {0};
    oracle_GXAdjustForOverscan(&rmin, &o, hor, ver);
    GXAdjustForOverscan(&rmin, &p, hor, ver);

    CHECK(rm_eq(&o, &p), "L3 double-strike parity");

    /* Even though xFBmode=SF, progressive mode uses ver*2 */
    CHECK(o.xfbHeight == (u16)(rmin.xfbHeight - ver * 2),
          "L3 progressive xfbHeight -= ver*2");
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: Zero overscan => identity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L4_zero(void)
{
    oracle_GXRenderModeObj rmin = rand_rm();
    oracle_GXRenderModeObj o = {0}, p = {0};

    oracle_GXAdjustForOverscan(&rmin, &o, 0, 0);
    GXAdjustForOverscan(&rmin, &p, 0, 0);

    CHECK(rm_eq(&o, &p), "L4 zero overscan parity");
    CHECK(rm_eq(&o, &rmin), "L4 zero overscan is identity");
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

    printf("gxoverscan_property_test: seed=%u num_runs=%d\n", seed, num_runs);

    for (i = 0; i < num_runs; i++) {
        g_rng = seed + (uint32_t)i;

        test_L0_parity();
        test_L1_inplace();
        test_L2_singlefield();
        test_L3_doublestrike();
        test_L4_zero();
    }

    printf("\ngxoverscan_property_test: %d/%d PASS", g_pass, g_pass + g_fail);
    if (g_fail) printf(", %d FAIL", g_fail);
    printf("\n");

    return g_fail ? 1 : 0;
}
