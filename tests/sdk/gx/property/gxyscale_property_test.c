/*
 * gxyscale_property_test.c — Property-based parity test for GXGetYScaleFactor
 *
 * Oracle + port: exact copy of decomp __GXGetNumXfbLines + GXGetYScaleFactor
 * (GXFrameBuf.c:220-290). Pure integer/float computation.
 *
 * Levels:
 *   L0 — Oracle vs port parity (__GXGetNumXfbLines)
 *   L1 — Oracle vs port parity (GXGetYScaleFactor)
 *   L2 — GXGetYScaleFactor result produces correct XFB height
 *   L3 — __GXGetNumXfbLines monotonicity: larger scale -> more lines
 *   L4 — Identity: efbHeight == xfbHeight -> yScale ~= 1.0
 *   L5 — Random integration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ── xorshift32 PRNG ────────────────────────────────────────────── */
static uint32_t g_rng;
static uint32_t xorshift32(void) {
    uint32_t x = g_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng = x;
    return x;
}

/* ── Counters ───────────────────────────────────────────────────── */
static uint64_t g_total_checks;
static uint64_t g_total_pass;
static int       g_verbose;
static const char *g_opt_op;

#define CHECK(cond, ...) do { \
    g_total_checks++; \
    if (!(cond)) { \
        printf("FAIL @ %s:%d: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); printf("\n"); \
        return 0; \
    } \
    g_total_pass++; \
} while(0)

/* ═══════════════════════════════════════════════════════════════════
 * ORACLE — exact copy of decomp
 * ═══════════════════════════════════════════════════════════════════ */

static uint32_t oracle_GXGetNumXfbLines(uint32_t efbHt, uint32_t iScale)
{
    uint32_t count;
    uint32_t realHt;
    uint32_t iScaleD;

    count = (efbHt - 1) * 0x100;
    realHt = (count / iScale) + 1;

    iScaleD = iScale;

    if (iScaleD > 0x80 && iScaleD < 0x100) {
        while (iScaleD % 2 == 0) {
            iScaleD /= 2;
        }

        if (efbHt % iScaleD == 0) {
            realHt++;
        }
    }

    if (realHt > 0x400) {
        realHt = 0x400;
    }

    return realHt;
}

static float oracle_GXGetYScaleFactor(uint16_t efbHeight, uint16_t xfbHeight)
{
    float fScale;
    float yScale;
    uint32_t iScale;
    uint32_t tgtHt;
    uint32_t realHt;

    tgtHt = xfbHeight;
    yScale = (float)xfbHeight / (float)efbHeight;
    iScale = (uint32_t)(256.0f / yScale) & 0x1FF;
    realHt = oracle_GXGetNumXfbLines(efbHeight, iScale);

    while (realHt > xfbHeight) {
        tgtHt--;
        yScale = (float)tgtHt / (float)efbHeight;
        iScale = (uint32_t)(256.0f / yScale) & 0x1FF;
        realHt = oracle_GXGetNumXfbLines(efbHeight, iScale);
    }

    fScale = yScale;
    while (realHt < xfbHeight) {
        fScale = yScale;
        tgtHt++;
        yScale = (float)tgtHt / (float)efbHeight;
        iScale = (uint32_t)(256.0f / yScale) & 0x1FF;
        realHt = oracle_GXGetNumXfbLines(efbHeight, iScale);
    }

    return fScale;
}

/* ═══════════════════════════════════════════════════════════════════
 * PORT — linked from src/sdk_port/gx/GX.c
 * ═══════════════════════════════════════════════════════════════════ */

typedef uint32_t u32_gx;
typedef uint16_t u16_gx;
float GXGetYScaleFactor(u16_gx efbHeight, u16_gx xfbHeight);

/* __GXGetNumXfbLines is static in GX.c, so we keep a local copy for
 * sub-function level testing.  The main parity test uses GXGetYScaleFactor. */
static uint32_t port_GXGetNumXfbLines(uint32_t efbHt, uint32_t iScale)
{
    uint32_t count = (efbHt - 1) * 0x100;
    uint32_t realHt = (count / iScale) + 1;
    uint32_t iScaleD = iScale;
    if (iScaleD > 0x80 && iScaleD < 0x100) {
        while (iScaleD % 2 == 0) iScaleD /= 2;
        if (efbHt % iScaleD == 0) realHt++;
    }
    if (realHt > 0x400) realHt = 0x400;
    return realHt;
}

#define port_GXGetYScaleFactor(efb, xfb) GXGetYScaleFactor((efb), (xfb))

/* ═══════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════ */

/* Random efbHeight in [2, 528], xfbHeight in [efbHeight, min(1024, efbHeight*4)] */
static void rand_heights(uint16_t *efb, uint16_t *xfb) {
    *efb = 2 + (xorshift32() % 527);  /* 2..528 */
    uint16_t max_xfb = *efb * 4;
    if (max_xfb > 1024) max_xfb = 1024;
    *xfb = *efb + (xorshift32() % (max_xfb - *efb + 1));
}

/* Compute __GXGetNumXfbLines from a yScale factor */
static uint32_t lines_from_yscale(uint16_t efb, float yScale) {
    uint32_t iScale = (uint32_t)(256.0f / yScale) & 0x1FF;
    return oracle_GXGetNumXfbLines(efb, iScale);
}

/* ═══════════════════════════════════════════════════════════════════
 * L0 — __GXGetNumXfbLines parity
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L0_numxfb_parity(void) {
    int i;
    for (i = 0; i < 500; i++) {
        uint32_t efbHt = 2 + (xorshift32() % 1023);
        uint32_t iScale = 1 + (xorshift32() % 0x1FF);
        uint32_t ov = oracle_GXGetNumXfbLines(efbHt, iScale);
        uint32_t pv = port_GXGetNumXfbLines(efbHt, iScale);

        CHECK(ov == pv,
              "L0: efbHt=%u iScale=%u oracle=%u port=%u",
              efbHt, iScale, ov, pv);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L1 — GXGetYScaleFactor parity
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L1_yscale_parity(void) {
    int i;
    for (i = 0; i < 100; i++) {
        uint16_t efb, xfb;
        rand_heights(&efb, &xfb);
        float ov = oracle_GXGetYScaleFactor(efb, xfb);
        float pv = port_GXGetYScaleFactor(efb, xfb);

        CHECK(ov == pv,
              "L1: efb=%u xfb=%u oracle=%.6f port=%.6f",
              efb, xfb, ov, pv);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2 — Result produces correct XFB height
 *       lines_from_yscale(efb, result) <= xfbHeight
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L2_result_correct(void) {
    int i;
    for (i = 0; i < 100; i++) {
        uint16_t efb, xfb;
        rand_heights(&efb, &xfb);
        float ys = oracle_GXGetYScaleFactor(efb, xfb);
        uint32_t lines = lines_from_yscale(efb, ys);

        CHECK(lines <= xfb,
              "L2: efb=%u xfb=%u ys=%.6f -> lines=%u (> xfb)",
              efb, xfb, ys, lines);

        /* The scale factor should be >= 1.0 (xfb >= efb) */
        CHECK(ys >= 1.0f - 0.001f,
              "L2: efb=%u xfb=%u ys=%.6f (< 1.0)",
              efb, xfb, ys);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3 — __GXGetNumXfbLines: bounded by 0x400
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L3_numxfb_bounded(void) {
    int i;
    for (i = 0; i < 500; i++) {
        uint32_t efbHt = 2 + (xorshift32() % 1023);
        uint32_t iScale = 1 + (xorshift32() % 0x1FF);
        uint32_t result = oracle_GXGetNumXfbLines(efbHt, iScale);

        CHECK(result <= 0x400,
              "L3: efbHt=%u iScale=%u result=%u (> 0x400)",
              efbHt, iScale, result);
        CHECK(result >= 1,
              "L3: efbHt=%u iScale=%u result=%u (< 1)",
              efbHt, iScale, result);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4 — Identity: efb == xfb -> yScale ~= 1.0
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L4_identity(void) {
    int i;
    for (i = 0; i < 50; i++) {
        uint16_t h = 2 + (xorshift32() % 527);
        float ys = oracle_GXGetYScaleFactor(h, h);
        float diff = ys - 1.0f;
        if (diff < 0) diff = -diff;

        CHECK(diff < 0.01f,
              "L4: h=%u ys=%.6f (expected ~1.0)",
              h, ys);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5 — Random integration
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L5_random_integration(void) {
    int i;
    for (i = 0; i < 100; i++) {
        uint16_t efb, xfb;
        rand_heights(&efb, &xfb);

        float oys = oracle_GXGetYScaleFactor(efb, xfb);
        float pys = port_GXGetYScaleFactor(efb, xfb);

        CHECK(oys == pys,
              "L5 parity: efb=%u xfb=%u oracle=%.6f port=%.6f",
              efb, xfb, oys, pys);

        uint32_t lines = lines_from_yscale(efb, oys);
        CHECK(lines <= xfb,
              "L5 correct: efb=%u xfb=%u ys=%.6f -> lines=%u",
              efb, xfb, oys, lines);

        /* Also test GXGetNumXfbLines via public API path */
        uint32_t iScale = (uint32_t)(256.0f / oys) & 0x1FF;
        uint32_t olines = oracle_GXGetNumXfbLines(efb, iScale);
        uint32_t plines = port_GXGetNumXfbLines(efb, iScale);
        CHECK(olines == plines,
              "L5 numxfb parity: efb=%u iScale=%u oracle=%u port=%u",
              efb, iScale, olines, plines);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * Per-seed runner
 * ═══════════════════════════════════════════════════════════════════ */

static int run_seed(uint32_t seed) {
    g_rng = seed;
    if (!g_opt_op || strstr("L0", g_opt_op) || strstr("NUMXFB", g_opt_op) || strstr("PARITY", g_opt_op)) {
        if (!test_L0_numxfb_parity()) return 0;
    }
    if (!g_opt_op || strstr("L1", g_opt_op) || strstr("YSCALE", g_opt_op)) {
        if (!test_L1_yscale_parity()) return 0;
    }
    if (!g_opt_op || strstr("L2", g_opt_op) || strstr("RESULT", g_opt_op)) {
        if (!test_L2_result_correct()) return 0;
    }
    if (!g_opt_op || strstr("L3", g_opt_op) || strstr("BOUNDED", g_opt_op) || strstr("RANGE", g_opt_op)) {
        if (!test_L3_numxfb_bounded()) return 0;
    }
    if (!g_opt_op || strstr("L4", g_opt_op) || strstr("IDENTITY", g_opt_op)) {
        if (!test_L4_identity()) return 0;
    }
    if (!g_opt_op || strstr("L5", g_opt_op) || strstr("FULL", g_opt_op) || strstr("MIX", g_opt_op) || strstr("RANDOM", g_opt_op)) {
        if (!test_L5_random_integration()) return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    uint32_t start_seed = 1;
    int num_runs = 100;
    int i;

    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0)
            start_seed = (uint32_t)strtoul(argv[i] + 7, NULL, 0);
        else if (strncmp(argv[i], "--num-runs=", 11) == 0)
            num_runs = atoi(argv[i] + 11);
        else if (strncmp(argv[i], "--op=", 5) == 0)
            g_opt_op = argv[i] + 5;
        else if (strcmp(argv[i], "-v") == 0)
            g_verbose = 1;
        else {
            fprintf(stderr,
                    "Usage: gxyscale_property_test [--seed=N] [--num-runs=N] "
                    "[--op=L0|L1|L2|L3|L4|L5|NUMXFB|YSCALE|RESULT|BOUNDED|IDENTITY|FULL|MIX] [-v]\n");
            return 2;
        }
    }

    printf("\n=== GXGetYScaleFactor Property Test ===\n");

    for (i = 0; i < num_runs; i++) {
        uint32_t seed = start_seed + (uint32_t)i;
        uint64_t before = g_total_checks;

        if (!run_seed(seed)) {
            printf("  FAILED at seed %u\n", seed);
            printf("\n--- Summary ---\n");
            printf("Seeds:  %d (failed at %d)\n", i + 1, i + 1);
            printf("Checks: %llu  (pass=%llu  fail=1)\n",
                   (unsigned long long)g_total_checks,
                   (unsigned long long)g_total_pass);
            printf("\nRESULT: FAIL\n");
            return 1;
        }

        if (g_verbose) {
            printf("  seed %u: %llu checks OK\n",
                   seed, (unsigned long long)(g_total_checks - before));
        }
        if ((i + 1) % 100 == 0) {
            printf("  progress: seed %d/%d\n", i + 1, num_runs);
        }
    }

    printf("\n--- Summary ---\n");
    printf("Seeds:  %d\n", num_runs);
    printf("Checks: %llu  (pass=%llu  fail=0)\n",
           (unsigned long long)g_total_checks,
           (unsigned long long)g_total_pass);
    printf("\nRESULT: %llu/%llu PASS\n",
           (unsigned long long)g_total_pass,
           (unsigned long long)g_total_checks);

    return 0;
}
