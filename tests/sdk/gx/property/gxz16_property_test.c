/*
 * gxz16_property_test.c — Property-based parity test for GXCompressZ16/GXDecompressZ16
 *
 * Oracle: exact copy of decomp GXCompressZ16 + GXDecompressZ16 (GXMisc.c:362-485)
 * Port:   identical logic (pure integer function, no gc_mem)
 *
 * Levels:
 *   L0 — Oracle vs port parity (compress + decompress)
 *   L1 — LINEAR round-trip: decompress(compress(z)) == z & 0xFFFF00
 *   L2 — Idempotence: compress(decompress(compress(z))) == compress(z)
 *   L3 — Bit range: z16 < 0x10000, z24 <= 0xFFFFFF
 *   L4 — Exhaustive LINEAR (all 65536 z16 values)
 *   L5 — Random integration mix across all formats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

/* ── Z format enums ─────────────────────────────────────────────── */
#define GX_ZC_LINEAR  0
#define GX_ZC_NEAR    1
#define GX_ZC_MID     2
#define GX_ZC_FAR     3

/* ── __cntlzw replacement ───────────────────────────────────────── */
static uint32_t my_cntlzw(uint32_t x) {
    if (x == 0) return 32;
#if defined(__GNUC__) || defined(__clang__)
    return (uint32_t)__builtin_clz(x);
#else
    uint32_t n = 0;
    if (x <= 0x0000FFFF) { n += 16; x <<= 16; }
    if (x <= 0x00FFFFFF) { n += 8;  x <<= 8;  }
    if (x <= 0x0FFFFFFF) { n += 4;  x <<= 4;  }
    if (x <= 0x3FFFFFFF) { n += 2;  x <<= 2;  }
    if (x <= 0x7FFFFFFF) { n += 1; }
    return n;
#endif
}

/* ═══════════════════════════════════════════════════════════════════
 * ORACLE — exact copy of decomp
 * ═══════════════════════════════════════════════════════════════════ */

static uint32_t oracle_GXCompressZ16(uint32_t z24, uint32_t zfmt)
{
    uint32_t z16;
    uint32_t z24n;
    int32_t exp;
    int32_t shift;
    int32_t temp;

    z24n = ~(z24 << 8);
    temp = (int32_t)my_cntlzw(z24n);
    switch (zfmt) {
        case GX_ZC_LINEAR:
            z16 = (z24 >> 8) & 0xFFFF;
            break;
        case GX_ZC_NEAR:
            if (temp > 3) {
                exp = 3;
            }
            else {
                exp = temp;
            }
            if (exp == 3) {
                shift = 7;
            }
            else {
                shift = 9 - exp;
            }
            z16 = ((z24 >> shift) & 0x3FFF & ~0xFFFFC000) | (exp << 14);
            break;
        case GX_ZC_MID:
            if (temp > 7) {
                exp = 7;
            }
            else {
                exp = temp;
            }
            if (exp == 7) {
                shift = 4;
            }
            else {
                shift = 10 - exp;
            }
            z16 = ((z24 >> shift) & 0x1FFF & ~0xFFFFE000) | (exp << 13);
            break;
        case GX_ZC_FAR:
            if (temp > 12) {
                exp = 12;
            }
            else {
                exp = temp;
            }
            if (exp == 12) {
                shift = 0;
            }
            else {
                shift = 11 - exp;
            }
            z16 = ((z24 >> shift) & 0xFFF & ~0xFFFFF000) | (exp << 12);
            break;
        default:
            z16 = 0;
            break;
    }
    return z16;
}

static uint32_t oracle_GXDecompressZ16(uint32_t z16, uint32_t zfmt)
{
    uint32_t z24;
    uint32_t cb1;
    int32_t exp;
    int32_t shift;

    switch (zfmt) {
        case GX_ZC_LINEAR:
            z24 = (z16 << 8) & 0xFFFF00;
            break;
        case GX_ZC_NEAR:
            exp = (z16 >> 14) & 3;
            if (exp == 3) {
                shift = 7;
            }
            else {
                shift = 9 - exp;
            }
            cb1 = (uint32_t)((int32_t)-1 << (24 - exp));
            z24 = (cb1 | ((z16 & 0x3FFF) << shift)) & 0xFFFFFF;
            break;
        case GX_ZC_MID:
            exp = (z16 >> 13) & 7;
            if (exp == 7) {
                shift = 4;
            }
            else {
                shift = 10 - exp;
            }
            cb1 = (uint32_t)((int32_t)-1 << (24 - exp));
            z24 = (cb1 | ((z16 & 0x1FFF) << shift)) & 0xFFFFFF;
            break;
        case GX_ZC_FAR:
            exp = (z16 >> 12) & 0xF;
            if (exp == 12) {
                shift = 0;
            }
            else {
                shift = 11 - exp;
            }
            cb1 = (uint32_t)((int32_t)-1 << (24 - exp));
            z24 = (cb1 | ((z16 & 0xFFF) << shift)) & 0xFFFFFF;
            break;
        default:
            z24 = 0;
            break;
    }
    return z24;
}

/* ═══════════════════════════════════════════════════════════════════
 * PORT — identical pure-integer implementation
 * ═══════════════════════════════════════════════════════════════════ */

static uint32_t port_GXCompressZ16(uint32_t z24, uint32_t zfmt)
{
    uint32_t z16;
    uint32_t z24n;
    int32_t exp;
    int32_t shift;
    int32_t temp;

    z24n = ~(z24 << 8);
    temp = (int32_t)my_cntlzw(z24n);
    switch (zfmt) {
        case GX_ZC_LINEAR:
            z16 = (z24 >> 8) & 0xFFFF;
            break;
        case GX_ZC_NEAR:
            exp = (temp > 3) ? 3 : temp;
            shift = (exp == 3) ? 7 : (9 - exp);
            z16 = ((z24 >> shift) & 0x3FFF) | (exp << 14);
            break;
        case GX_ZC_MID:
            exp = (temp > 7) ? 7 : temp;
            shift = (exp == 7) ? 4 : (10 - exp);
            z16 = ((z24 >> shift) & 0x1FFF) | (exp << 13);
            break;
        case GX_ZC_FAR:
            exp = (temp > 12) ? 12 : temp;
            shift = (exp == 12) ? 0 : (11 - exp);
            z16 = ((z24 >> shift) & 0xFFF) | (exp << 12);
            break;
        default:
            z16 = 0;
            break;
    }
    return z16;
}

static uint32_t port_GXDecompressZ16(uint32_t z16, uint32_t zfmt)
{
    uint32_t z24;
    uint32_t cb1;
    int32_t exp;
    int32_t shift;

    switch (zfmt) {
        case GX_ZC_LINEAR:
            z24 = (z16 << 8) & 0xFFFF00;
            break;
        case GX_ZC_NEAR:
            exp = (z16 >> 14) & 3;
            shift = (exp == 3) ? 7 : (9 - exp);
            cb1 = (uint32_t)((int32_t)-1 << (24 - exp));
            z24 = (cb1 | ((z16 & 0x3FFF) << shift)) & 0xFFFFFF;
            break;
        case GX_ZC_MID:
            exp = (z16 >> 13) & 7;
            shift = (exp == 7) ? 4 : (10 - exp);
            cb1 = (uint32_t)((int32_t)-1 << (24 - exp));
            z24 = (cb1 | ((z16 & 0x1FFF) << shift)) & 0xFFFFFF;
            break;
        case GX_ZC_FAR:
            exp = (z16 >> 12) & 0xF;
            shift = (exp == 12) ? 0 : (11 - exp);
            cb1 = (uint32_t)((int32_t)-1 << (24 - exp));
            z24 = (cb1 | ((z16 & 0xFFF) << shift)) & 0xFFFFFF;
            break;
        default:
            z24 = 0;
            break;
    }
    return z24;
}

/* ═══════════════════════════════════════════════════════════════════
 * Random z24 value (24 bits)
 * ═══════════════════════════════════════════════════════════════════ */
static uint32_t rand_z24(void) {
    return xorshift32() & 0xFFFFFF;
}

static uint32_t rand_z16(void) {
    return xorshift32() & 0xFFFF;
}

static uint32_t rand_fmt(void) {
    return xorshift32() % 4;
}

/* ═══════════════════════════════════════════════════════════════════
 * L0 — Oracle vs port parity
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L0_parity(void) {
    int i;
    for (i = 0; i < 500; i++) {
        uint32_t z24 = rand_z24();
        uint32_t fmt = rand_fmt();
        uint32_t oc  = oracle_GXCompressZ16(z24, fmt);
        uint32_t pc  = port_GXCompressZ16(z24, fmt);

        CHECK(oc == pc,
              "L0 compress parity: z24=0x%06X fmt=%u oracle=0x%04X port=0x%04X",
              z24, fmt, oc, pc);
    }
    for (i = 0; i < 500; i++) {
        uint32_t z16 = rand_z16();
        uint32_t fmt = rand_fmt();
        uint32_t od  = oracle_GXDecompressZ16(z16, fmt);
        uint32_t pd  = port_GXDecompressZ16(z16, fmt);

        CHECK(od == pd,
              "L0 decompress parity: z16=0x%04X fmt=%u oracle=0x%06X port=0x%06X",
              z16, fmt, od, pd);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L1 — LINEAR round-trip: decompress(compress(z)) == z & 0xFFFF00
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L1_linear_roundtrip(void) {
    int i;
    for (i = 0; i < 200; i++) {
        uint32_t z24 = rand_z24();
        uint32_t z16 = oracle_GXCompressZ16(z24, GX_ZC_LINEAR);
        uint32_t rt  = oracle_GXDecompressZ16(z16, GX_ZC_LINEAR);
        uint32_t expected = z24 & 0xFFFF00;

        CHECK(rt == expected,
              "L1 LINEAR roundtrip: z24=0x%06X -> z16=0x%04X -> rt=0x%06X expected=0x%06X",
              z24, z16, rt, expected);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2 — Idempotence: compress(decompress(compress(z))) == compress(z)
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L2_idempotence(void) {
    int i;
    for (i = 0; i < 500; i++) {
        uint32_t z24 = rand_z24();
        uint32_t fmt = rand_fmt();
        uint32_t c1  = oracle_GXCompressZ16(z24, fmt);
        uint32_t d1  = oracle_GXDecompressZ16(c1, fmt);
        uint32_t c2  = oracle_GXCompressZ16(d1, fmt);

        CHECK(c1 == c2,
              "L2 idempotence: z24=0x%06X fmt=%u c1=0x%04X d1=0x%06X c2=0x%04X",
              z24, fmt, c1, d1, c2);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3 — Bit range: z16 < 0x10000, z24 <= 0xFFFFFF
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L3_bit_range(void) {
    int i;
    for (i = 0; i < 500; i++) {
        uint32_t z24 = rand_z24();
        uint32_t fmt = rand_fmt();
        uint32_t z16 = oracle_GXCompressZ16(z24, fmt);

        CHECK(z16 <= 0xFFFF,
              "L3 z16 range: z24=0x%06X fmt=%u z16=0x%X (> 0xFFFF)",
              z24, fmt, z16);

        uint32_t rt = oracle_GXDecompressZ16(z16, fmt);
        CHECK(rt <= 0xFFFFFF,
              "L3 z24 range: z16=0x%04X fmt=%u z24=0x%X (> 0xFFFFFF)",
              z16, fmt, rt);
    }
    /* Also test decompress of arbitrary z16 values */
    for (i = 0; i < 500; i++) {
        uint32_t z16 = rand_z16();
        uint32_t fmt = rand_fmt();
        uint32_t z24 = oracle_GXDecompressZ16(z16, fmt);

        CHECK(z24 <= 0xFFFFFF,
              "L3 decompress z24 range: z16=0x%04X fmt=%u z24=0x%X",
              z16, fmt, z24);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4 — Exhaustive LINEAR: all 65536 z16 values round-trip correctly
 *       (only run once per seed, not parameterized)
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L4_exhaustive_linear(void) {
    uint32_t z16;
    for (z16 = 0; z16 <= 0xFFFF; z16++) {
        uint32_t z24 = oracle_GXDecompressZ16(z16, GX_ZC_LINEAR);
        uint32_t rt  = oracle_GXCompressZ16(z24, GX_ZC_LINEAR);

        CHECK(rt == z16,
              "L4 exhaustive LINEAR: z16=0x%04X -> z24=0x%06X -> rt=0x%04X",
              z16, z24, rt);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5 — Random integration: all formats, compress+decompress+re-compress
 * ═══════════════════════════════════════════════════════════════════ */

static int test_L5_random_integration(void) {
    int i;
    for (i = 0; i < 500; i++) {
        uint32_t z24 = rand_z24();
        uint32_t fmt = rand_fmt();
        uint32_t oc  = oracle_GXCompressZ16(z24, fmt);
        uint32_t pc  = port_GXCompressZ16(z24, fmt);
        uint32_t od  = oracle_GXDecompressZ16(oc, fmt);
        uint32_t pd  = port_GXDecompressZ16(pc, fmt);
        uint32_t oc2 = oracle_GXCompressZ16(od, fmt);
        uint32_t pc2 = port_GXCompressZ16(pd, fmt);

        /* Parity at every step */
        CHECK(oc == pc,
              "L5 compress: z24=0x%06X fmt=%u oracle=0x%04X port=0x%04X",
              z24, fmt, oc, pc);
        CHECK(od == pd,
              "L5 decompress: z16=0x%04X fmt=%u oracle=0x%06X port=0x%06X",
              oc, fmt, od, pd);
        CHECK(oc2 == pc2,
              "L5 re-compress: z24=0x%06X fmt=%u oracle=0x%04X port=0x%04X",
              od, fmt, oc2, pc2);
        /* Idempotence */
        CHECK(oc == oc2,
              "L5 idempotence: fmt=%u c1=0x%04X c2=0x%04X",
              fmt, oc, oc2);
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
    if (!g_opt_op || strstr("L1", g_opt_op) || strstr("ROUNDTRIP", g_opt_op)) {
        if (!test_L1_linear_roundtrip()) return 0;
    }
    if (!g_opt_op || strstr("L2", g_opt_op) || strstr("IDEMPOTENCE", g_opt_op)) {
        if (!test_L2_idempotence()) return 0;
    }
    if (!g_opt_op || strstr("L3", g_opt_op) || strstr("RANGE", g_opt_op)) {
        if (!test_L3_bit_range()) return 0;
    }
    if (!g_opt_op || strstr("L4", g_opt_op) || strstr("EXHAUSTIVE", g_opt_op)) {
        if (!test_L4_exhaustive_linear()) return 0;
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
                    "Usage: gxz16_property_test [--seed=N] [--num-runs=N] "
                    "[--op=L0|L1|L2|L3|L4|L5|PARITY|ROUNDTRIP|IDEMPOTENCE|RANGE|EXHAUSTIVE|FULL|MIX] [-v]\n");
            return 2;
        }
    }

    printf("\n=== GXCompressZ16/GXDecompressZ16 Property Test ===\n");

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
