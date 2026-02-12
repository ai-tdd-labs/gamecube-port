/*
 * ar_property_test.c — Property-based parity test for AR allocator.
 *
 * Oracle: exact copy of decomp ar.c (native structs, host memory)
 * Port:   linked from src/sdk_port/ar/ar.c (gc_mem big-endian access)
 *
 * Levels:
 *   L0 — Single alloc/free parity
 *   L1 — Sequential allocs, verify SP growth
 *   L2 — Alloc-then-free LIFO, verify SP returns to 0x4000
 *   L3 — Random alloc/free mix, compare all state
 *   L4 — Edge cases (full capacity, free output param)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* sdk_port headers */
#include "ar.h"
#include "gc_mem.h"

/* ── xorshift32 PRNG ────────────────────────────────────────────── */
static uint32_t g_rng;
static uint32_t xorshift32(void) {
    uint32_t x = g_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g_rng = x;
    return x;
}

/* ═══════════════════════════════════════════════════════════════════
 * ORACLE — exact copy of decomp ar.c (native structs)
 * ═══════════════════════════════════════════════════════════════════ */

#define ORACLE_MAX_BLOCKS 64

static uint32_t oracle_AR_StackPointer;
static uint32_t oracle_AR_FreeBlocks;
static uint32_t oracle_AR_BlockLength[ORACLE_MAX_BLOCKS];
static uint32_t *oracle_AR_BlockLengthPtr;
static uint32_t oracle_AR_Size;
static int      oracle_AR_init_flag;

/* ARInit (ar.c:103-135) — stripped hardware */
static uint32_t oracle_ARInit(uint32_t num_entries, uint32_t aramSize) {
    if (oracle_AR_init_flag == 1) {
        return 0x4000;
    }

    oracle_AR_StackPointer = 0x4000;
    oracle_AR_FreeBlocks = num_entries;
    oracle_AR_BlockLengthPtr = oracle_AR_BlockLength;
    oracle_AR_Size = aramSize;
    oracle_AR_init_flag = 1;

    return oracle_AR_StackPointer;
}

/* ARAlloc (ar.c:61-75) */
static uint32_t oracle_ARAlloc(uint32_t length) {
    uint32_t tmp;

    tmp = oracle_AR_StackPointer;
    oracle_AR_StackPointer += length;
    *oracle_AR_BlockLengthPtr = length;
    oracle_AR_BlockLengthPtr++;
    oracle_AR_FreeBlocks--;

    return tmp;
}

/* ARFree (ar.c:77-96) */
static uint32_t oracle_ARFree(uint32_t *length) {
    oracle_AR_BlockLengthPtr--;

    if (length) {
        *length = *oracle_AR_BlockLengthPtr;
    }

    oracle_AR_StackPointer -= *oracle_AR_BlockLengthPtr;
    oracle_AR_FreeBlocks++;

    return oracle_AR_StackPointer;
}

/* ARCheckInit (ar.c:98-101) */
static int oracle_ARCheckInit(void) {
    return oracle_AR_init_flag;
}

/* ARGetBaseAddress (ar.c:139-142) */
static uint32_t oracle_ARGetBaseAddress(void) {
    return 0x4000;
}

/* ARGetSize (ar.c:144-147) */
static uint32_t oracle_ARGetSize(void) {
    return oracle_AR_Size;
}

/* ═══════════════════════════════════════════════════════════════════
 * PORT — linked from sdk_port/ar/ar.c (gc_mem big-endian)
 * ═══════════════════════════════════════════════════════════════════ */

#define PORT_MAX_BLOCKS ORACLE_MAX_BLOCKS

/* gc_mem backing for block length array */
#define GC_BLOCKLENGTH_BASE 0x80001000u
#define GC_BLOCKLENGTH_SIZE (PORT_MAX_BLOCKS * 4)

static uint8_t gc_ar_backing[GC_BLOCKLENGTH_SIZE];

/* Host-side AR state */
static port_ARState port_state;

/* BE helper for test reads */
static uint32_t test_load_u32be(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

/* ═══════════════════════════════════════════════════════════════════
 * TEST INFRASTRUCTURE
 * ═══════════════════════════════════════════════════════════════════ */

static int g_verbose;
static int g_pass, g_fail;

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); \
        g_fail++; return; \
    } else { g_pass++; } \
} while (0)

static void init_both(uint32_t numEntries, uint32_t aramSize) {
    /* Reset oracle */
    oracle_AR_StackPointer = 0;
    oracle_AR_FreeBlocks = 0;
    memset(oracle_AR_BlockLength, 0, sizeof(oracle_AR_BlockLength));
    oracle_AR_BlockLengthPtr = oracle_AR_BlockLength;
    oracle_AR_Size = 0;
    oracle_AR_init_flag = 0;

    /* Reset gc_mem backing and re-register */
    memset(gc_ar_backing, 0, sizeof(gc_ar_backing));
    gc_mem_set(GC_BLOCKLENGTH_BASE, GC_BLOCKLENGTH_SIZE, gc_ar_backing);

    /* Reset port state */
    memset(&port_state, 0, sizeof(port_state));

    /* Init both */
    oracle_ARInit(numEntries, aramSize);
    port_ARInit(&port_state, GC_BLOCKLENGTH_BASE, numEntries, aramSize);
}

static void compare_state(const char *label) {
    CHECK(oracle_AR_StackPointer == port_state.stackPointer,
          "%s: SP oracle=%u port=%u",
          label, oracle_AR_StackPointer, port_state.stackPointer);
    CHECK(oracle_AR_FreeBlocks == port_state.freeBlocks,
          "%s: FreeBlocks oracle=%u port=%u",
          label, oracle_AR_FreeBlocks, port_state.freeBlocks);
    CHECK(oracle_AR_init_flag == port_state.initFlag,
          "%s: initFlag oracle=%d port=%d",
          label, oracle_AR_init_flag, port_state.initFlag);
    CHECK(oracle_AR_Size == port_state.size,
          "%s: Size oracle=%u port=%u",
          label, oracle_AR_Size, port_state.size);

    /* Compare block length array contents */
    uint32_t used = (uint32_t)(oracle_AR_BlockLengthPtr - oracle_AR_BlockLength);
    CHECK(used == port_state.blockLengthIdx,
          "%s: blockLengthIdx oracle=%u port=%u",
          label, used, port_state.blockLengthIdx);

    for (uint32_t i = 0; i < used; i++) {
        uint32_t port_val = test_load_u32be(GC_BLOCKLENGTH_BASE + i * 4);
        CHECK(oracle_AR_BlockLength[i] == port_val,
              "%s: BlockLength[%u] oracle=%u port=%u",
              label, i, oracle_AR_BlockLength[i], port_val);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * TEST LEVELS
 * ═══════════════════════════════════════════════════════════════════ */

/* L0: Single alloc/free parity */
static void test_L0_single(void) {
    uint32_t len = (xorshift32() % 0x10000 + 1) & ~3u; /* 4-aligned */
    if (len == 0) len = 4;

    init_both(ORACLE_MAX_BLOCKS, 0x1000000);

    uint32_t o_ret = oracle_ARAlloc(len);
    uint32_t p_ret = port_ARAlloc(&port_state, len);
    CHECK(o_ret == p_ret, "L0 alloc ret: oracle=%u port=%u", o_ret, p_ret);

    compare_state("L0 after alloc");

    uint32_t o_len = 0, p_len = 0;
    uint32_t o_sp = oracle_ARFree(&o_len);
    uint32_t p_sp = port_ARFree(&port_state, &p_len);
    CHECK(o_sp == p_sp, "L0 free ret: oracle=%u port=%u", o_sp, p_sp);
    CHECK(o_len == p_len, "L0 free len: oracle=%u port=%u", o_len, p_len);

    compare_state("L0 after free");
}

/* L1: Sequential allocs */
static void test_L1_sequential(void) {
    uint32_t n = xorshift32() % 32 + 1;
    init_both(ORACLE_MAX_BLOCKS, 0x1000000);

    for (uint32_t i = 0; i < n; i++) {
        uint32_t len = (xorshift32() % 0x8000 + 1) & ~3u;
        if (len == 0) len = 4;

        uint32_t o_ret = oracle_ARAlloc(len);
        uint32_t p_ret = port_ARAlloc(&port_state, len);
        CHECK(o_ret == p_ret, "L1 alloc[%u] ret: oracle=%u port=%u", i, o_ret, p_ret);
    }

    compare_state("L1 after allocs");
}

/* L2: Alloc-then-free LIFO, SP should return to 0x4000 */
static void test_L2_lifo(void) {
    uint32_t n = xorshift32() % 32 + 1;
    init_both(ORACLE_MAX_BLOCKS, 0x1000000);

    for (uint32_t i = 0; i < n; i++) {
        uint32_t len = (xorshift32() % 0x8000 + 1) & ~3u;
        if (len == 0) len = 4;
        oracle_ARAlloc(len);
        port_ARAlloc(&port_state, len);
    }

    for (uint32_t i = 0; i < n; i++) {
        uint32_t o_len = 0, p_len = 0;
        uint32_t o_sp = oracle_ARFree(&o_len);
        uint32_t p_sp = port_ARFree(&port_state, &p_len);
        CHECK(o_sp == p_sp, "L2 free[%u] ret: oracle=%u port=%u", i, o_sp, p_sp);
        CHECK(o_len == p_len, "L2 free[%u] len: oracle=%u port=%u", i, o_len, p_len);
    }

    CHECK(oracle_AR_StackPointer == 0x4000,
          "L2 SP should be 0x4000 after full LIFO, got %u", oracle_AR_StackPointer);
    compare_state("L2 after full LIFO");
}

/* L3: Random alloc/free mix */
static void test_L3_random_mix(void) {
    init_both(ORACLE_MAX_BLOCKS, 0x1000000);
    uint32_t alloc_count = 0;
    uint32_t ops = xorshift32() % 60 + 10;

    for (uint32_t i = 0; i < ops; i++) {
        int do_alloc;
        if (alloc_count == 0) do_alloc = 1;
        else if (alloc_count >= ORACLE_MAX_BLOCKS - 1) do_alloc = 0;
        else do_alloc = (xorshift32() % 3 != 0); /* 2/3 chance alloc */

        if (do_alloc) {
            uint32_t len = (xorshift32() % 0x4000 + 1) & ~3u;
            if (len == 0) len = 4;

            uint32_t o_ret = oracle_ARAlloc(len);
            uint32_t p_ret = port_ARAlloc(&port_state, len);
            CHECK(o_ret == p_ret, "L3 alloc ret: oracle=%u port=%u", o_ret, p_ret);
            alloc_count++;
        } else {
            uint32_t o_len = 0, p_len = 0;
            uint32_t o_sp = oracle_ARFree(&o_len);
            uint32_t p_sp = port_ARFree(&port_state, &p_len);
            CHECK(o_sp == p_sp, "L3 free ret: oracle=%u port=%u", o_sp, p_sp);
            CHECK(o_len == p_len, "L3 free len: oracle=%u port=%u", o_len, p_len);
            alloc_count--;
        }
    }

    compare_state("L3 after random mix");
}

/* L4: Getters and init parity */
static void test_L4_getters(void) {
    uint32_t aramSize = 0x1000000 + (xorshift32() % 0x1000000);
    init_both(ORACLE_MAX_BLOCKS, aramSize);

    CHECK(oracle_ARCheckInit() == port_ARCheckInit(&port_state),
          "L4 CheckInit: oracle=%d port=%d",
          oracle_ARCheckInit(), port_ARCheckInit(&port_state));
    CHECK(oracle_ARGetBaseAddress() == port_ARGetBaseAddress(&port_state),
          "L4 GetBaseAddress: oracle=%u port=%u",
          oracle_ARGetBaseAddress(), port_ARGetBaseAddress(&port_state));
    CHECK(oracle_ARGetSize() == port_ARGetSize(&port_state),
          "L4 GetSize: oracle=%u port=%u",
          oracle_ARGetSize(), port_ARGetSize(&port_state));

    /* Free with NULL length param */
    oracle_ARAlloc(0x100);
    port_ARAlloc(&port_state, 0x100);

    uint32_t o_sp = oracle_ARFree(NULL);
    uint32_t p_sp = port_ARFree(&port_state, NULL);
    CHECK(o_sp == p_sp, "L4 free(NULL) ret: oracle=%u port=%u", o_sp, p_sp);

    compare_state("L4 after free(NULL)");
}

/* ═══════════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    int num_runs = 2000;
    uint32_t seed = 12345;
    g_verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (sscanf(argv[i], "--seed=%u", &seed) == 1) continue;
        if (sscanf(argv[i], "--num-runs=%d", &num_runs) == 1) continue;
        if (strcmp(argv[i], "-v") == 0) { g_verbose = 1; continue; }
    }

    printf("ar_property_test: seed=%u num_runs=%d\n", seed, num_runs);

    for (int run = 0; run < num_runs; run++) {
        g_rng = seed + (uint32_t)run;

        test_L0_single();
        test_L1_sequential();
        test_L2_lifo();
        test_L3_random_mix();
        test_L4_getters();
    }

    printf("\nar_property_test: %d/%d PASS", g_pass, g_pass + g_fail);
    if (g_fail) printf(", %d FAIL", g_fail);
    printf("\n");

    return g_fail ? 1 : 0;
}
