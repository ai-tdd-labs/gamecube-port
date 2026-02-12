#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtx_oracle.h"

typedef float f32;
typedef unsigned int u32;
typedef f32 Mtx[3][4];
typedef f32 ROMtx[4][3];
typedef struct {
    f32 x, y, z;
} Vec;

extern void PSMTXReorder(const Mtx src, ROMtx dest);
extern void PSMTXROMultVecArray(const ROMtx m, const Vec *srcBase, Vec *dstBase, u32 count);
extern void PSMTXMultVecArray(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count);

static uint32_t rng_state = 1u;

static uint32_t rng_next(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static float rng_f32(float lo, float hi) {
    const float t = (float)(rng_next() & 0x00FFFFFFu) / 16777215.0f;
    return lo + (hi - lo) * t;
}

static int feq(float a, float b) {
    if (a == b) return 1;
    float diff = fabsf(a - b);
    float scale = fmaxf(1.0f, fmaxf(fabsf(a), fabsf(b)));
    return (diff / scale) < 1e-5f;
}

static void fill_mtx(oracle_Mtx om, Mtx pm) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            float v = rng_f32(-10.0f, 10.0f);
            om[r][c] = v;
            pm[r][c] = v;
        }
    }
}

static void fill_vecs(oracle_Vec *ov, Vec *pv, u32 count) {
    for (u32 i = 0; i < count; i++) {
        float x = rng_f32(-20.0f, 20.0f);
        float y = rng_f32(-20.0f, 20.0f);
        float z = rng_f32(-20.0f, 20.0f);
        ov[i].x = x; ov[i].y = y; ov[i].z = z;
        pv[i].x = x; pv[i].y = y; pv[i].z = z;
    }
}

static int run_l0_reorder(uint32_t seed, int verbose) {
    oracle_Mtx om;
    Mtx pm;
    oracle_ROMtx od;
    ROMtx pd;
    (void)verbose;
    rng_state = seed ? seed : 1u;
    fill_mtx(om, pm);
    oracle_PSMTXReorder(om, od);
    PSMTXReorder(pm, pd);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            if (!feq(od[r][c], pd[r][c])) return 0;
        }
    }
    return 1;
}

static int run_l1_romult(uint32_t seed, u32 steps, int verbose) {
    oracle_Mtx om;
    Mtx pm;
    oracle_ROMtx orm;
    ROMtx prm;
    (void)verbose;
    rng_state = seed ? seed : 1u;
    fill_mtx(om, pm);
    oracle_PSMTXReorder(om, orm);
    PSMTXReorder(pm, prm);

    oracle_Vec osrc[16], odst[16];
    Vec psrc[16], pdst[16];
    u32 count = 1u + (steps % 16u);
    fill_vecs(osrc, psrc, count);
    oracle_PSMTXROMultVecArray(orm, osrc, odst, count);
    PSMTXROMultVecArray(prm, psrc, pdst, count);
    for (u32 i = 0; i < count; i++) {
        if (!feq(odst[i].x, pdst[i].x) || !feq(odst[i].y, pdst[i].y) || !feq(odst[i].z, pdst[i].z)) {
            return 0;
        }
    }
    return 1;
}

static int run_l2_multvecarray(uint32_t seed, u32 steps, int verbose) {
    oracle_Mtx om;
    Mtx pm;
    (void)verbose;
    rng_state = seed ? seed : 1u;
    fill_mtx(om, pm);
    oracle_Vec osrc[16], odst[16];
    Vec psrc[16], pdst[16];
    u32 count = 1u + (steps % 16u);
    fill_vecs(osrc, psrc, count);
    oracle_PSMTXMultVecArray(om, osrc, odst, count);
    PSMTXMultVecArray(pm, psrc, pdst, count);
    for (u32 i = 0; i < count; i++) {
        if (!feq(odst[i].x, pdst[i].x) || !feq(odst[i].y, pdst[i].y) || !feq(odst[i].z, pdst[i].z)) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    uint32_t seed = 0xC0DEC0DEu;
    u32 num_runs = 2000u;
    u32 steps = 32u;
    int verbose = 0;
    const char *op = "FULL";

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) seed = (uint32_t)strtoul(argv[i] + 7, NULL, 0);
        else if (strncmp(argv[i], "--num-runs=", 11) == 0) num_runs = (u32)strtoul(argv[i] + 11, NULL, 0);
        else if (strncmp(argv[i], "--steps=", 8) == 0) steps = (u32)strtoul(argv[i] + 8, NULL, 0);
        else if (strncmp(argv[i], "--op=", 5) == 0) op = argv[i] + 5;
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) verbose = 1;
    }

    u32 pass = 0, total = 0;
    for (u32 i = 0; i < num_runs; i++) {
        uint32_t s = seed + i * 0x9E3779B9u;
        if (strcmp(op, "L0") == 0 || strcmp(op, "FULL") == 0) {
            total++;
            if (run_l0_reorder(s, verbose)) pass++;
        }
        if (strcmp(op, "L1") == 0 || strcmp(op, "FULL") == 0) {
            total++;
            if (run_l1_romult(s ^ 0x13579BDFu, steps + i, verbose)) pass++;
        }
        if (strcmp(op, "L2") == 0 || strcmp(op, "FULL") == 0) {
            total++;
            if (run_l2_multvecarray(s ^ 0x2468ACE0u, steps + i, verbose)) pass++;
        }
    }

    if (pass != total) {
        fprintf(stderr, "psmtx_batch_property_test: %u/%u PASS\n", pass, total);
        return 1;
    }

    printf("psmtx_batch_property_test: %u/%u PASS\n", pass, total);
    return 0;
}
