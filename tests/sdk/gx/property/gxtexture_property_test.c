/*
 * gxtexture_property_test.c — Property-based parity test for GXGetTexBufferSize
 *
 * Oracle: exact copy of decomp __GXGetTexTileShift + GXGetTexBufferSize
 * Port:   identical logic (this is a pure function, so port == oracle;
 *         we verify properties and cross-check against manual computation)
 *
 * Levels:
 *   L0 — Oracle vs port parity for random inputs
 *   L1 — Non-mipmap: bufferSize = ceil(w/tileW) * ceil(h/tileH) * tileBytes
 *   L2 — Mipmap monotonicity: more LOD levels => larger or equal buffer
 *   L3 — RGBA8 uses 64-byte tiles (double other formats)
 *   L4 — GetImageTileCount parity and properties
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

/* ═══════════════════════════════════════════════════════════════════
 * GX texture format constants (from GX headers)
 * ═══════════════════════════════════════════════════════════════════ */

/* _GX_TF_CTF = 0x20, _GX_TF_ZTF = 0x10 */

/* Standard texture formats */
#define GX_TF_I4       0x0
#define GX_TF_I8       0x1
#define GX_TF_IA4      0x2
#define GX_TF_IA8      0x3
#define GX_TF_RGB565   0x4
#define GX_TF_RGB5A3   0x5
#define GX_TF_RGBA8    0x6
#define GX_TF_CMPR     0xE

/* Z texture formats (0x10 | base) */
#define GX_TF_Z8       0x11
#define GX_TF_Z16      0x13
#define GX_TF_Z24X8    0x16

/* Copy texture formats (0x20 | base) */
#define GX_CTF_R4      0x20  /* 0x0 | 0x20 */
#define GX_CTF_RA4     0x22  /* 0x2 | 0x20 */
#define GX_CTF_RA8     0x23  /* 0x3 | 0x20 */
#define GX_CTF_A8      0x27  /* 0x7 | 0x20 */
#define GX_TF_A8       0x27  /* = GX_CTF_A8 */
#define GX_CTF_R8      0x28  /* 0x8 | 0x20 */
#define GX_CTF_G8      0x29  /* 0x9 | 0x20 */
#define GX_CTF_B8      0x2A  /* 0xA | 0x20 */
#define GX_CTF_RG8     0x2B  /* 0xB | 0x20 */
#define GX_CTF_GB8     0x2C  /* 0xC | 0x20 */

/* Z copy texture formats (0x10 | 0x20 | base) */
#define GX_CTF_Z4      0x30  /* 0x0 | 0x30 */
#define GX_CTF_Z8M     0x39  /* 0x9 | 0x30 */
#define GX_CTF_Z8L     0x3A  /* 0xA | 0x30 */
#define GX_CTF_Z16L    0x3C  /* 0xC | 0x30 */

/* __cntlzw replacement — count leading zeros for 32-bit word */
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

static void oracle_GXGetTexTileShift(uint32_t fmt, uint32_t *rowTileS, uint32_t *colTileS) {
    switch (fmt) {
    case GX_TF_I4:
    case 0x8:
    case GX_TF_CMPR:
    case GX_CTF_R4:
    case GX_CTF_Z4:
        *rowTileS = 3; *colTileS = 3; break;
    case GX_TF_I8:
    case GX_TF_IA4:
    case 0x9:
    case GX_TF_Z8:
    case GX_CTF_RA4:
    case GX_TF_A8:
    case GX_CTF_R8:
    case GX_CTF_G8:
    case GX_CTF_B8:
    case GX_CTF_Z8M:
    case GX_CTF_Z8L:
        *rowTileS = 3; *colTileS = 2; break;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_RGBA8:
    case 0xA:
    case GX_TF_Z16:
    case GX_TF_Z24X8:
    case GX_CTF_RA8:
    case GX_CTF_RG8:
    case GX_CTF_GB8:
    case GX_CTF_Z16L:
        *rowTileS = 2; *colTileS = 2; break;
    default:
        *rowTileS = *colTileS = 0; break;
    }
}

static uint32_t oracle_GXGetTexBufferSize(uint16_t width, uint16_t height,
                                           uint32_t format, uint8_t mipmap, uint8_t max_lod) {
    uint32_t tileShiftX, tileShiftY;
    uint32_t tileBytes;
    uint32_t bufferSize;
    uint32_t nx, ny;
    uint32_t level;

    oracle_GXGetTexTileShift(format, &tileShiftX, &tileShiftY);
    if (format == GX_TF_RGBA8 || format == GX_TF_Z24X8) {
        tileBytes = 64;
    } else {
        tileBytes = 32;
    }

    if (mipmap == 1) {
        bufferSize = 0;
        for (level = 0; level < max_lod; level++) {
            nx = (width + (1 << tileShiftX) - 1) >> tileShiftX;
            ny = (height + (1 << tileShiftY) - 1) >> tileShiftY;
            bufferSize += tileBytes * (nx * ny);
            if (width == 1 && height == 1) break;
            width = (width > 1) ? width >> 1 : 1;
            height = (height > 1) ? height >> 1 : 1;
        }
    } else {
        nx = (width + (1 << tileShiftX) - 1) >> tileShiftX;
        ny = (height + (1 << tileShiftY) - 1) >> tileShiftY;
        bufferSize = nx * ny * tileBytes;
    }
    return bufferSize;
}

static void oracle_GetImageTileCount(uint32_t fmt, uint16_t wd, uint16_t ht,
                                      uint32_t *rowTiles, uint32_t *colTiles, uint32_t *cmpTiles) {
    uint32_t texRowShift, texColShift;
    oracle_GXGetTexTileShift(fmt, &texRowShift, &texColShift);
    if (wd == 0) wd = 1;
    if (ht == 0) ht = 1;
    *rowTiles = (wd + (1 << texRowShift) - 1) >> texRowShift;
    *colTiles = (ht + (1 << texColShift) - 1) >> texColShift;
    *cmpTiles = (fmt == GX_TF_RGBA8 || fmt == GX_TF_Z24X8) ? 2 : 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * PORT — same logic (pure function, no gc_mem needed)
 * ═══════════════════════════════════════════════════════════════════ */

/* For a pure computation like this, the port IS the oracle.
 * We still implement it separately to catch copy-paste errors
 * and verify our understanding of the algorithm. */

static void port_GXGetTexTileShift(uint32_t fmt, uint32_t *rowTileS, uint32_t *colTileS) {
    switch (fmt) {
    case GX_TF_I4:   case 0x8: case GX_TF_CMPR: case GX_CTF_R4: case GX_CTF_Z4:
        *rowTileS = 3; *colTileS = 3; break;
    case GX_TF_I8:   case GX_TF_IA4: case 0x9: case GX_TF_Z8:
    case GX_CTF_RA4: case GX_TF_A8:  case GX_CTF_R8: case GX_CTF_G8:
    case GX_CTF_B8:  case GX_CTF_Z8M: case GX_CTF_Z8L:
        *rowTileS = 3; *colTileS = 2; break;
    case GX_TF_IA8:  case GX_TF_RGB565: case GX_TF_RGB5A3: case GX_TF_RGBA8:
    case 0xA: case GX_TF_Z16: case GX_TF_Z24X8: case GX_CTF_RA8:
    case GX_CTF_RG8: case GX_CTF_GB8: case GX_CTF_Z16L:
        *rowTileS = 2; *colTileS = 2; break;
    default:
        *rowTileS = *colTileS = 0; break;
    }
}

/* port_GXGetTexBufferSize — linked from src/sdk_port/gx/GX.c */
typedef uint32_t u32_gx;
typedef uint16_t u16_gx;
typedef uint8_t u8_gx;
u32_gx GXGetTexBufferSize(u16_gx width, u16_gx height, u32_gx format, u8_gx mipmap, u8_gx max_lod);
#define port_GXGetTexBufferSize(w, h, fmt, mm, ml) GXGetTexBufferSize((w), (h), (fmt), (mm), (ml))

static void port_GetImageTileCount(uint32_t fmt, uint16_t wd, uint16_t ht,
                                    uint32_t *rowTiles, uint32_t *colTiles, uint32_t *cmpTiles) {
    uint32_t texRowShift, texColShift;
    port_GXGetTexTileShift(fmt, &texRowShift, &texColShift);
    if (wd == 0) wd = 1;
    if (ht == 0) ht = 1;
    *rowTiles = (wd + (1 << texRowShift) - 1) >> texRowShift;
    *colTiles = (ht + (1 << texColShift) - 1) >> texColShift;
    *cmpTiles = (fmt == GX_TF_RGBA8 || fmt == GX_TF_Z24X8) ? 2 : 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * Test helpers
 * ═══════════════════════════════════════════════════════════════════ */

/* Valid texture formats for random selection */
static const uint32_t VALID_FORMATS[] = {
    GX_TF_I4, GX_TF_I8, GX_TF_IA4, GX_TF_IA8,
    GX_TF_RGB565, GX_TF_RGB5A3, GX_TF_RGBA8, GX_TF_CMPR,
    GX_TF_Z8, GX_TF_Z16, GX_TF_Z24X8,
    0x8, 0x9, 0xA,
};
#define NUM_VALID_FORMATS (sizeof(VALID_FORMATS) / sizeof(VALID_FORMATS[0]))

static uint32_t random_format(void) {
    return VALID_FORMATS[xorshift32() % NUM_VALID_FORMATS];
}

/* Generate power-of-2 dimensions for mipmap mode */
static uint16_t random_pow2_dim(void) {
    /* 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 */
    uint32_t exp = xorshift32() % 11;
    return (uint16_t)(1u << exp);
}

static uint16_t random_dim(void) {
    /* 1..1024 */
    return (uint16_t)(1 + (xorshift32() % 1024));
}

/* ═══════════════════════════════════════════════════════════════════
 * L0 — Oracle vs port parity
 * ═══════════════════════════════════════════════════════════════════ */
static int test_parity(uint32_t seed) {
    int i, n;
    g_rng = seed;

    n = 20 + (xorshift32() % 40);
    for (i = 0; i < n; i++) {
        uint32_t fmt = random_format();
        uint8_t mipmap = (xorshift32() % 2) ? 1 : 0;
        uint16_t w = mipmap ? random_pow2_dim() : random_dim();
        uint16_t h = mipmap ? random_pow2_dim() : random_dim();
        uint8_t max_lod = mipmap ? (uint8_t)(1 + (xorshift32() % 11)) : 1;

        uint32_t os = oracle_GXGetTexBufferSize(w, h, fmt, mipmap, max_lod);
        uint32_t ps = port_GXGetTexBufferSize(w, h, fmt, mipmap, max_lod);
        CHECK(os == ps, "L0 parity: fmt=0x%x w=%u h=%u mm=%u lod=%u: oracle=%u port=%u",
              fmt, w, h, mipmap, max_lod, os, ps);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L1 — Non-mipmap: verify against manual tile computation
 * ═══════════════════════════════════════════════════════════════════ */
static int test_non_mipmap(uint32_t seed) {
    int i, n;
    g_rng = seed;

    n = 20 + (xorshift32() % 30);
    for (i = 0; i < n; i++) {
        uint32_t fmt = random_format();
        uint16_t w = random_dim();
        uint16_t h = random_dim();
        uint32_t rowS, colS;
        uint32_t tileBytes, expected;
        uint32_t actual;

        oracle_GXGetTexTileShift(fmt, &rowS, &colS);
        tileBytes = (fmt == GX_TF_RGBA8 || fmt == GX_TF_Z24X8) ? 64 : 32;

        /* Manual: ceil(w / (1<<rowS)) * ceil(h / (1<<colS)) * tileBytes */
        {
            uint32_t tileW = 1 << rowS;
            uint32_t tileH = 1 << colS;
            uint32_t tilesX = (w + tileW - 1) / tileW;
            uint32_t tilesY = (h + tileH - 1) / tileH;
            expected = tilesX * tilesY * tileBytes;
        }

        actual = oracle_GXGetTexBufferSize(w, h, fmt, 0, 1);
        CHECK(actual == expected,
              "L1 non-mipmap: fmt=0x%x w=%u h=%u: got=%u expected=%u",
              fmt, w, h, actual, expected);
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2 — Mipmap monotonicity: more LOD levels => larger or equal buffer
 * ═══════════════════════════════════════════════════════════════════ */
static int test_mipmap_monotonic(uint32_t seed) {
    int i, n;
    g_rng = seed;

    n = 20 + (xorshift32() % 30);
    for (i = 0; i < n; i++) {
        uint32_t fmt = random_format();
        uint16_t w = random_pow2_dim();
        uint16_t h = random_pow2_dim();
        uint32_t prev = 0;
        uint8_t lod;

        for (lod = 1; lod <= 11; lod++) {
            uint32_t s = oracle_GXGetTexBufferSize(w, h, fmt, 1, lod);
            CHECK(s >= prev,
                  "L2 monotonic: fmt=0x%x w=%u h=%u lod=%u: %u < prev %u",
                  fmt, w, h, lod, s, prev);
            prev = s;
        }
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3 — RGBA8/Z24X8 tiles are double size
 * ═══════════════════════════════════════════════════════════════════ */
static int test_rgba8_double(uint32_t seed) {
    int i, n;
    g_rng = seed;

    n = 15 + (xorshift32() % 20);
    for (i = 0; i < n; i++) {
        uint16_t w = random_dim();
        uint16_t h = random_dim();
        uint32_t rowS_rgba, colS_rgba, rowS_ia8, colS_ia8;

        oracle_GXGetTexTileShift(GX_TF_RGBA8, &rowS_rgba, &colS_rgba);
        oracle_GXGetTexTileShift(GX_TF_IA8, &rowS_ia8, &colS_ia8);

        /* Both RGBA8 and IA8 have tile shift 2,2 but RGBA8 has 64-byte tiles */
        CHECK(rowS_rgba == rowS_ia8 && colS_rgba == colS_ia8,
              "L3 tile shifts should match for RGBA8 and IA8");

        {
            uint32_t s_rgba = oracle_GXGetTexBufferSize(w, h, GX_TF_RGBA8, 0, 1);
            uint32_t s_ia8 = oracle_GXGetTexBufferSize(w, h, GX_TF_IA8, 0, 1);
            CHECK(s_rgba == 2 * s_ia8,
                  "L3 RGBA8 double: w=%u h=%u rgba8=%u ia8=%u", w, h, s_rgba, s_ia8);
        }
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4 — GetImageTileCount parity and properties
 * ═══════════════════════════════════════════════════════════════════ */
static int test_tile_count(uint32_t seed) {
    int i, n;
    g_rng = seed;

    n = 20 + (xorshift32() % 30);
    for (i = 0; i < n; i++) {
        uint32_t fmt = random_format();
        uint16_t w = random_dim();
        uint16_t h = random_dim();
        uint32_t or_row, or_col, or_cmp;
        uint32_t pr_row, pr_col, pr_cmp;

        oracle_GetImageTileCount(fmt, w, h, &or_row, &or_col, &or_cmp);
        port_GetImageTileCount(fmt, w, h, &pr_row, &pr_col, &pr_cmp);

        CHECK(or_row == pr_row && or_col == pr_col && or_cmp == pr_cmp,
              "L4 parity: fmt=0x%x w=%u h=%u: oracle=(%u,%u,%u) port=(%u,%u,%u)",
              fmt, w, h, or_row, or_col, or_cmp, pr_row, pr_col, pr_cmp);

        /* Property: tile count >= 1 */
        CHECK(or_row >= 1 && or_col >= 1, "L4: tile count < 1");

        /* Property: cmpTiles is 1 or 2 */
        CHECK(or_cmp == 1 || or_cmp == 2, "L4: cmpTiles=%u", or_cmp);

        /* Property: non-mipmap buffer = rowTiles * colTiles * tileBytes * cmpTiles */
        {
            uint32_t tileBytes = 32;
            uint32_t expected = or_row * or_col * tileBytes * or_cmp;
            uint32_t actual = oracle_GXGetTexBufferSize(w, h, fmt, 0, 1);
            CHECK(actual == expected,
                  "L4 cross-check: fmt=0x%x w=%u h=%u: tiles=%u*%u*%u*%u=%u actual=%u",
                  fmt, w, h, or_row, or_col, tileBytes, or_cmp, expected, actual);
        }
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * Runner
 * ═══════════════════════════════════════════════════════════════════ */

static int run_seed(uint32_t seed) {
    uint32_t sub;
    g_rng = seed;
    sub = xorshift32();

    if (!g_opt_op || strstr("L0", g_opt_op) || strstr("PARITY", g_opt_op)) {
        if (!test_parity(sub ^ 0x7EF00001u)) return 0;
    }
    if (!g_opt_op || strstr("L1", g_opt_op) || strstr("NOMIP", g_opt_op)) {
        if (!test_non_mipmap(sub ^ 0x7EF00002u)) return 0;
    }
    if (!g_opt_op || strstr("L2", g_opt_op) || strstr("MONO", g_opt_op)) {
        if (!test_mipmap_monotonic(sub ^ 0x7EF00003u)) return 0;
    }
    if (!g_opt_op || strstr("L3", g_opt_op) || strstr("RGBA8", g_opt_op)) {
        if (!test_rgba8_double(sub ^ 0x7EF00004u)) return 0;
    }
    if (!g_opt_op || strstr("L4", g_opt_op) || strstr("TILE", g_opt_op)) {
        if (!test_tile_count(sub ^ 0x7EF00005u)) return 0;
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
                    "Usage: gxtexture_property_test [--seed=N] [--num-runs=N] "
                    "[--op=L0|L1|L2|L3|L4|PARITY|NOMIP|MONO|RGBA8|TILE] [-v]\n");
            return 2;
        }
    }

    printf("\n=== GXTexture Property Test ===\n");

    for (i = 0; i < num_runs; i++) {
        uint32_t seed = start_seed + (uint32_t)i;
        uint64_t before = g_total_checks;

        if (!run_seed(seed)) {
            printf("  FAILED at seed %u\n", seed);
            printf("\n--- Summary ---\n");
            printf("Seeds:  %d (failed at %d)\n", i + 1, i + 1);
            printf("Checks: %llu  (pass=%llu  fail=1)\n",
                   g_total_checks, g_total_pass);
            printf("\nRESULT: FAIL\n");
            return 1;
        }

        if (g_verbose) {
            printf("  seed %u: %llu checks OK\n",
                   seed, g_total_checks - before);
        }
        if ((i + 1) % 100 == 0) {
            printf("  progress: seed %d/%d\n", i + 1, num_runs);
        }
    }

    printf("\n--- Summary ---\n");
    printf("Seeds:  %d\n", num_runs);
    printf("Checks: %llu  (pass=%llu  fail=0)\n",
           g_total_checks, g_total_pass);
    printf("\nRESULT: %llu/%llu PASS\n", g_total_pass, g_total_checks);

    return 0;
}
