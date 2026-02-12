/*
 * mtx_batch_property_test.c — PBT for PSMTXMultVecArray, PSMTXReorder,
 * PSMTXROMultVecArray.
 *
 * Properties tested:
 *   P1: PSMTXMultVecArray(m, vecs, out, n) == loop of C_MTXMultVec
 *   P2: PSMTXReorder(m, rom); PSMTXROMultVecArray(rom, vecs, out, n)
 *       == PSMTXMultVecArray(m, vecs, out, n)
 *
 * Usage:
 *   mtx_batch_property_test [--seed=N] [--num-runs=N] [-v]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef float f32;
typedef unsigned int u32;
typedef int s32;
typedef f32 Mtx[3][4];
typedef f32 ROMtx[3][4];
typedef struct { f32 x, y, z; } Vec;

extern void C_MTXMultVec(const Mtx m, const Vec *src, Vec *dst);
extern void PSMTXMultVecArray(const Mtx m, const Vec *srcBase, Vec *dstBase, u32 count);
extern void PSMTXReorder(const Mtx src, ROMtx dest);
extern void PSMTXROMultVecArray(const ROMtx m, const Vec *srcBase, Vec *dstBase, u32 count);

#define MAX_VECS 64
#define EPS 1e-5f

static u32 g_seed;
static int g_verbose;

static u32 xorshift32(void) {
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}

static f32 randf(void) {
    return (f32)(s32)(xorshift32() & 0xFFFF) / 100.0f - 327.0f;
}

static void rand_mtx(Mtx m) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++)
            m[r][c] = randf();
}

static void rand_vecs(Vec *v, u32 n) {
    for (u32 i = 0; i < n; i++) {
        v[i].x = randf();
        v[i].y = randf();
        v[i].z = randf();
    }
}

static int vec_eq(const Vec *a, const Vec *b) {
    return fabsf(a->x - b->x) < EPS &&
           fabsf(a->y - b->y) < EPS &&
           fabsf(a->z - b->z) < EPS;
}

static int test_multvec_array(u32 seed) {
    g_seed = seed;

    Mtx m;
    rand_mtx(m);

    u32 count = (xorshift32() % MAX_VECS) + 1;
    Vec src[MAX_VECS], dst_loop[MAX_VECS], dst_batch[MAX_VECS];
    rand_vecs(src, count);

    /* Oracle: C_MTXMultVec in a loop. */
    for (u32 i = 0; i < count; i++) {
        C_MTXMultVec(m, &src[i], &dst_loop[i]);
    }

    /* Port: PSMTXMultVecArray. */
    memset(dst_batch, 0, sizeof(dst_batch));
    PSMTXMultVecArray(m, src, dst_batch, count);

    for (u32 i = 0; i < count; i++) {
        if (!vec_eq(&dst_loop[i], &dst_batch[i])) {
            if (g_verbose) {
                printf("  P1 FAIL at vec %u: loop=(%g,%g,%g) batch=(%g,%g,%g)\n",
                       i, dst_loop[i].x, dst_loop[i].y, dst_loop[i].z,
                       dst_batch[i].x, dst_batch[i].y, dst_batch[i].z);
            }
            return 0;
        }
    }
    return 1;
}

static int test_reorder_romultvec(u32 seed) {
    g_seed = seed;

    Mtx m;
    rand_mtx(m);

    u32 count = (xorshift32() % MAX_VECS) + 1;
    Vec src[MAX_VECS], dst_normal[MAX_VECS], dst_ro[MAX_VECS];
    rand_vecs(src, count);

    /* Oracle: PSMTXMultVecArray with the original matrix. */
    PSMTXMultVecArray(m, src, dst_normal, count);

    /* Port: Reorder + ROMultVecArray. */
    ROMtx rom;
    PSMTXReorder(m, rom);
    memset(dst_ro, 0, sizeof(dst_ro));
    PSMTXROMultVecArray(rom, src, dst_ro, count);

    for (u32 i = 0; i < count; i++) {
        if (!vec_eq(&dst_normal[i], &dst_ro[i])) {
            if (g_verbose) {
                printf("  P2 FAIL at vec %u: normal=(%g,%g,%g) ro=(%g,%g,%g)\n",
                       i, dst_normal[i].x, dst_normal[i].y, dst_normal[i].z,
                       dst_ro[i].x, dst_ro[i].y, dst_ro[i].z);
            }
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    u32 num_runs = 2000;
    u32 fixed_seed = 0;
    int has_seed = 0;
    g_verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            fixed_seed = (u32)atoi(argv[i] + 7);
            has_seed = 1;
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            num_runs = (u32)atoi(argv[i] + 11);
        } else if (strcmp(argv[i], "-v") == 0) {
            g_verbose = 1;
        }
    }

    u32 pass1 = 0, fail1 = 0;
    u32 pass2 = 0, fail2 = 0;
    u32 base_seed = has_seed ? fixed_seed : (u32)time(NULL);

    printf("MTX batch PBT: %u runs, base_seed=%u\n", num_runs, base_seed);

    for (u32 r = 0; r < num_runs; r++) {
        u32 seed = has_seed ? fixed_seed : base_seed + r;

        if (test_multvec_array(seed)) pass1++;
        else {
            fail1++;
            if (g_verbose) printf("  P1 fail seed=%u\n", seed);
        }

        if (test_reorder_romultvec(seed)) pass2++;
        else {
            fail2++;
            if (g_verbose) printf("  P2 fail seed=%u\n", seed);
        }
    }

    printf("P1 PSMTXMultVecArray:     %u/%u pass\n", pass1, num_runs);
    printf("P2 Reorder+ROMultVecArray: %u/%u pass\n", pass2, num_runs);

    int ok = (fail1 == 0 && fail2 == 0);
    printf("\n%s\n", ok ? "ALL PASS" : "FAIL");
    return ok ? 0 : 1;
}
