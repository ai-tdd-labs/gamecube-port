/*
 * gxproject_property_test.c — Property-based parity test for GXProject
 *
 * Oracle: exact copy of decomp GXProject (GXTransform.c:7-35)
 * Port:   identical logic (pure float function, no gc_mem)
 *
 * Levels:
 *   L0 — Oracle vs port parity for random inputs (perspective + orthographic)
 *   L1 — Identity modelview: eye coords == world coords
 *   L2 — Orthographic linearity: linear mapping from eye to screen
 *   L3 — Perspective: sx/sy scale inversely with -peye.z
 *   L4 — Viewport center: origin in eye space maps to viewport center (ortho)
 *   L5 — Random integration mix
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

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

/* Random float in [lo, hi] */
static float rand_float(float lo, float hi) {
    uint32_t r = xorshift32();
    float t = (float)(r & 0xFFFFFF) / (float)0xFFFFFF;
    return lo + t * (hi - lo);
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

/* Float comparison with epsilon */
static int feq(float a, float b, float eps) {
    float d = a - b;
    if (d < 0) d = -d;
    return d <= eps;
}

/* ═══════════════════════════════════════════════════════════════════
 * ORACLE — exact copy of decomp GXProject (GXTransform.c:7-35)
 * ═══════════════════════════════════════════════════════════════════ */

static void oracle_GXProject(float x, float y, float z,
                              const float mtx[3][4], const float *pm,
                              const float *vp,
                              float *sx, float *sy, float *sz)
{
    float peye_x, peye_y, peye_z;
    float xc, yc, zc, wc;

    /* modelview transform */
    peye_x = mtx[0][3] + ((mtx[0][2] * z) + ((mtx[0][0] * x) + (mtx[0][1] * y)));
    peye_y = mtx[1][3] + ((mtx[1][2] * z) + ((mtx[1][0] * x) + (mtx[1][1] * y)));
    peye_z = mtx[2][3] + ((mtx[2][2] * z) + ((mtx[2][0] * x) + (mtx[2][1] * y)));

    if (pm[0] == 0.0f) {
        /* perspective */
        xc = (peye_x * pm[1]) + (peye_z * pm[2]);
        yc = (peye_y * pm[3]) + (peye_z * pm[4]);
        zc = pm[6] + (peye_z * pm[5]);
        wc = 1.0f / -peye_z;
    } else {
        /* orthographic */
        xc = pm[2] + (peye_x * pm[1]);
        yc = pm[4] + (peye_y * pm[3]);
        zc = pm[6] + (peye_z * pm[5]);
        wc = 1.0f;
    }

    *sx = (vp[2] / 2.0f) + (vp[0] + (wc * (xc * vp[2] / 2.0f)));
    *sy = (vp[3] / 2.0f) + (vp[1] + (wc * (-yc * vp[3] / 2.0f)));
    *sz = vp[5] + (wc * (zc * (vp[5] - vp[4])));
}

/* ═══════════════════════════════════════════════════════════════════
 * PORT — linked from src/sdk_port/gx/GX.c (GXProject)
 * ═══════════════════════════════════════════════════════════════════ */

typedef float f32;
void GXProject(f32 x, f32 y, f32 z,
               const f32 mtx[3][4], const f32 *pm, const f32 *vp,
               f32 *sx, f32 *sy, f32 *sz);

/* ═══════════════════════════════════════════════════════════════════
 * Helper: generate random test inputs
 * ═══════════════════════════════════════════════════════════════════ */

/* Identity 3x4 matrix */
static void make_identity_mtx(float mtx[3][4]) {
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            mtx[i][j] = (i == j) ? 1.0f : 0.0f;
}

/* Random 3x4 matrix with elements in [-scale, scale] */
static void make_random_mtx(float mtx[3][4], float scale) {
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            mtx[i][j] = rand_float(-scale, scale);
}

/* Orthographic projection vector: pm[0]=1 (non-zero), pm[1..6] */
static void make_ortho_pm(float pm[7], float l, float r, float b, float t,
                           float n, float f) {
    pm[0] = 1.0f; /* orthographic flag */
    pm[1] = 2.0f / (r - l);
    pm[2] = -(r + l) / (r - l);
    pm[3] = 2.0f / (t - b);
    pm[4] = -(t + b) / (t - b);
    pm[5] = -1.0f / (f - n);
    pm[6] = -n / (f - n);
}

/* Perspective projection vector: pm[0]=0 */
static void make_persp_pm(float pm[7], float n, float f, float fovy_scale) {
    pm[0] = 0.0f; /* perspective flag */
    pm[1] = fovy_scale;        /* cot(fovy/2) / aspect */
    pm[2] = 0.0f;              /* x shear */
    pm[3] = fovy_scale;        /* cot(fovy/2) */
    pm[4] = 0.0f;              /* y shear */
    pm[5] = -f / (f - n);
    pm[6] = -(f * n) / (f - n);
}

/* Random viewport: [left, top, width, height, nearZ, farZ] */
static void make_random_vp(float vp[6]) {
    vp[0] = rand_float(0, 200);         /* left */
    vp[1] = rand_float(0, 150);         /* top */
    vp[2] = rand_float(100, 640);       /* width */
    vp[3] = rand_float(100, 480);       /* height */
    vp[4] = rand_float(0.0f, 0.5f);     /* nearZ */
    vp[5] = rand_float(0.5f, 1.0f);     /* farZ */
}

/* Standard 640x480 viewport */
static void make_standard_vp(float vp[6]) {
    vp[0] = 0.0f;
    vp[1] = 0.0f;
    vp[2] = 640.0f;
    vp[3] = 480.0f;
    vp[4] = 0.0f;
    vp[5] = 1.0f;
}

/* ═══════════════════════════════════════════════════════════════════
 * L0 — Oracle vs port parity for random inputs
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L0_parity(void) {
    int i;
    for (i = 0; i < 200; i++) {
        float mtx[3][4], pm[7], vp[6];
        float x, y, z;
        float osx, osy, osz, psx, psy, psz;
        int use_persp = (xorshift32() & 1);

        make_random_mtx(mtx, 10.0f);
        make_random_vp(vp);

        if (use_persp) {
            float n = rand_float(0.1f, 10.0f);
            float f = n + rand_float(10.0f, 1000.0f);
            make_persp_pm(pm, n, f, rand_float(0.5f, 3.0f));
            /* For perspective, ensure eye-z is negative (in front of camera) */
            x = rand_float(-5.0f, 5.0f);
            y = rand_float(-5.0f, 5.0f);
            z = rand_float(-100.0f, -1.0f);
        } else {
            make_ortho_pm(pm, -10, 10, -10, 10, 0.1f, 100.0f);
            x = rand_float(-10.0f, 10.0f);
            y = rand_float(-10.0f, 10.0f);
            z = rand_float(-100.0f, 100.0f);
        }

        oracle_GXProject(x, y, z, mtx, pm, vp, &osx, &osy, &osz);
        GXProject(x, y, z, mtx, pm, vp, &psx, &psy, &psz);

        /* Bit-exact parity since both are identical float code */
        CHECK(osx == psx && osy == psy && osz == psz,
              "L0 parity: seed iter %d: oracle=(%.6f,%.6f,%.6f) port=(%.6f,%.6f,%.6f)",
              i, osx, osy, osz, psx, psy, psz);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L1 — Identity modelview: eye coords == world coords
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L1_identity_modelview(void) {
    int i;
    float mtx[3][4];
    make_identity_mtx(mtx);

    for (i = 0; i < 100; i++) {
        float pm[7], vp[6];
        float x, y, z;
        float sx, sy, sz;
        float xc, yc, zc, wc;
        float expect_sx, expect_sy, expect_sz;

        make_standard_vp(vp);
        make_ortho_pm(pm, -10, 10, -7.5f, 7.5f, 0.1f, 100.0f);

        x = rand_float(-10.0f, 10.0f);
        y = rand_float(-7.5f, 7.5f);
        z = rand_float(-100.0f, -0.1f);

        oracle_GXProject(x, y, z, mtx, pm, vp, &sx, &sy, &sz);

        /* With identity modelview, peye == (x,y,z) */
        /* Ortho: xc = pm[2] + x*pm[1], yc = pm[4] + y*pm[3], wc = 1 */
        xc = pm[2] + (x * pm[1]);
        yc = pm[4] + (y * pm[3]);
        zc = pm[6] + (z * pm[5]);
        wc = 1.0f;

        expect_sx = (vp[2] / 2.0f) + (vp[0] + (wc * (xc * vp[2] / 2.0f)));
        expect_sy = (vp[3] / 2.0f) + (vp[1] + (wc * (-yc * vp[3] / 2.0f)));
        expect_sz = vp[5] + (wc * (zc * (vp[5] - vp[4])));

        CHECK(sx == expect_sx && sy == expect_sy && sz == expect_sz,
              "L1 identity: iter %d: got=(%.6f,%.6f,%.6f) expect=(%.6f,%.6f,%.6f)",
              i, sx, sy, sz, expect_sx, expect_sy, expect_sz);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2 — Orthographic linearity
 *       f(a*P1 + b*P2) = a*f(P1) + b*f(P2) - (a+b-1)*f(origin)
 *       Simplified: midpoint maps to midpoint of screen coords
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L2_ortho_linearity(void) {
    int i;
    float mtx[3][4], pm[7], vp[6];

    make_identity_mtx(mtx);
    make_standard_vp(vp);
    make_ortho_pm(pm, -10, 10, -10, 10, 0.1f, 100.0f);

    for (i = 0; i < 100; i++) {
        float x1 = rand_float(-9.0f, 9.0f), y1 = rand_float(-9.0f, 9.0f), z1 = rand_float(-90.0f, -1.0f);
        float x2 = rand_float(-9.0f, 9.0f), y2 = rand_float(-9.0f, 9.0f), z2 = rand_float(-90.0f, -1.0f);
        float mx = (x1 + x2) * 0.5f, my = (y1 + y2) * 0.5f, mz = (z1 + z2) * 0.5f;
        float sx1, sy1, sz1, sx2, sy2, sz2, smx, smy, smz;
        float emx, emy, emz;

        oracle_GXProject(x1, y1, z1, mtx, pm, vp, &sx1, &sy1, &sz1);
        oracle_GXProject(x2, y2, z2, mtx, pm, vp, &sx2, &sy2, &sz2);
        oracle_GXProject(mx, my, mz, mtx, pm, vp, &smx, &smy, &smz);

        /* Ortho is affine: midpoint in world -> midpoint in screen */
        emx = (sx1 + sx2) * 0.5f;
        emy = (sy1 + sy2) * 0.5f;
        emz = (sz1 + sz2) * 0.5f;

        CHECK(feq(smx, emx, 0.01f) && feq(smy, emy, 0.01f) && feq(smz, emz, 0.01f),
              "L2 ortho linearity: iter %d: mid=(%.4f,%.4f,%.4f) expected=(%.4f,%.4f,%.4f)",
              i, smx, smy, smz, emx, emy, emz);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3 — Perspective: moving point further away (more negative z)
 *       brings sx,sy closer to viewport center (converges to vanishing point)
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L3_perspective_depth(void) {
    int i;
    float mtx[3][4], pm[7], vp[6];
    float cx, cy; /* viewport center */

    make_identity_mtx(mtx);
    make_standard_vp(vp);
    make_persp_pm(pm, 1.0f, 1000.0f, 1.5f);

    cx = vp[0] + vp[2] / 2.0f;
    cy = vp[1] + vp[3] / 2.0f;

    for (i = 0; i < 100; i++) {
        float x = rand_float(-2.0f, 2.0f);
        float y = rand_float(-2.0f, 2.0f);
        float z_near = rand_float(-10.0f, -2.0f);
        float z_far  = z_near - rand_float(10.0f, 100.0f);
        float sx_near, sy_near, sz_near, sx_far, sy_far, sz_far;
        float d_near, d_far;

        oracle_GXProject(x, y, z_near, mtx, pm, vp, &sx_near, &sy_near, &sz_near);
        oracle_GXProject(x, y, z_far,  mtx, pm, vp, &sx_far,  &sy_far,  &sz_far);

        /* Distance from viewport center should decrease with depth */
        d_near = (sx_near - cx) * (sx_near - cx) + (sy_near - cy) * (sy_near - cy);
        d_far  = (sx_far  - cx) * (sx_far  - cx) + (sy_far  - cy) * (sy_far  - cy);

        CHECK(d_far <= d_near + 0.01f,
              "L3 persp depth: iter %d: d_near=%.4f d_far=%.4f (far should be <= near)",
              i, d_near, d_far);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4 — Viewport center: origin in eye space maps to viewport center (ortho)
 *       With symmetric ortho and identity modelview, (0,0,z) -> center
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L4_viewport_center(void) {
    int i;

    for (i = 0; i < 50; i++) {
        float mtx[3][4], pm[7], vp[6];
        float sx, sy, sz;
        float half = rand_float(1.0f, 20.0f);
        float cx, cy;

        make_identity_mtx(mtx);
        make_random_vp(vp);
        /* Symmetric ortho: [-half, half, -half, half, near, far] */
        make_ortho_pm(pm, -half, half, -half, half, 0.1f, 100.0f);

        cx = vp[0] + vp[2] / 2.0f;
        cy = vp[1] + vp[3] / 2.0f;

        /* Project origin */
        oracle_GXProject(0.0f, 0.0f, -50.0f, mtx, pm, vp, &sx, &sy, &sz);

        CHECK(feq(sx, cx, 0.01f) && feq(sy, cy, 0.01f),
              "L4 center: iter %d: got=(%.4f,%.4f) expected=(%.4f,%.4f)",
              i, sx, sy, cx, cy);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5 — Random integration: mixed perspective/ortho, random matrices,
 *       verify basic sanity (no NaN, no inf for valid inputs)
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L5_random_integration(void) {
    int i;

    for (i = 0; i < 200; i++) {
        float mtx[3][4], pm[7], vp[6];
        float x, y, z;
        float osx, osy, osz, psx, psy, psz;
        int use_persp = (xorshift32() % 3) != 0; /* 2/3 perspective, 1/3 ortho */

        make_random_mtx(mtx, 5.0f);
        make_random_vp(vp);

        if (use_persp) {
            float n = rand_float(0.1f, 5.0f);
            float f = n + rand_float(10.0f, 500.0f);
            float fov_scale = rand_float(0.3f, 4.0f);
            make_persp_pm(pm, n, f, fov_scale);
            /* Ensure valid perspective: object in front of camera after transform.
             * We generate z such that peye.z is likely negative. Since the matrix
             * is random, we just skip NaN/inf results gracefully. */
            x = rand_float(-3.0f, 3.0f);
            y = rand_float(-3.0f, 3.0f);
            z = rand_float(-50.0f, -1.0f);
        } else {
            float h = rand_float(1.0f, 20.0f);
            make_ortho_pm(pm, -h, h, -h, h, 0.1f, 100.0f);
            x = rand_float(-h, h);
            y = rand_float(-h, h);
            z = rand_float(-100.0f, 100.0f);
        }

        oracle_GXProject(x, y, z, mtx, pm, vp, &osx, &osy, &osz);
        GXProject(x, y, z, mtx, pm, vp, &psx, &psy, &psz);

        /* Bit-exact parity */
        CHECK(osx == psx && osy == psy && osz == psz,
              "L5 parity: iter %d: oracle=(%.6f,%.6f,%.6f) port=(%.6f,%.6f,%.6f)",
              i, osx, osy, osz, psx, psy, psz);

        /* No NaN check (only for finite inputs — random mtx might cause div-by-zero
         * in perspective mode, which is expected; skip NaN check for perspective
         * with random matrices since peye.z can be 0) */
        if (!use_persp) {
            CHECK(!isnan(osx) && !isnan(osy) && !isnan(osz),
                  "L5 NaN in ortho: iter %d: (%.6f,%.6f,%.6f)",
                  i, osx, osy, osz);
        }
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * Per-seed runner
 * ═══════════════════════════════════════════════════════════════════ */

static int run_seed(uint32_t seed) {
    g_rng = seed;
    if (!g_opt_op || strstr("L0", g_opt_op) || strstr("PARITY", g_opt_op)) {
        if (!test_L0_parity()) return 0;
    }
    if (!g_opt_op || strstr("L1", g_opt_op) || strstr("IDENTITY", g_opt_op)) {
        if (!test_L1_identity_modelview()) return 0;
    }
    if (!g_opt_op || strstr("L2", g_opt_op) || strstr("ORTHO", g_opt_op)) {
        if (!test_L2_ortho_linearity()) return 0;
    }
    if (!g_opt_op || strstr("L3", g_opt_op) || strstr("DEPTH", g_opt_op) || strstr("PERSPECTIVE", g_opt_op)) {
        if (!test_L3_perspective_depth()) return 0;
    }
    if (!g_opt_op || strstr("L4", g_opt_op) || strstr("CENTER", g_opt_op) || strstr("VIEWPORT", g_opt_op)) {
        if (!test_L4_viewport_center()) return 0;
    }
    if (!g_opt_op || strstr("L5", g_opt_op) || strstr("FULL", g_opt_op) || strstr("MIX", g_opt_op) || strstr("RANDOM", g_opt_op)) {
        if (!test_L5_random_integration()) return 0;
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════ */

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
                    "Usage: gxproject_property_test [--seed=N] [--num-runs=N] "
                    "[--op=L0|L1|L2|L3|L4|L5|PARITY|IDENTITY|ORTHO|DEPTH|CENTER|FULL|MIX] [-v]\n");
            return 2;
        }
    }

    printf("\n=== GXProject Property Test ===\n");

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
