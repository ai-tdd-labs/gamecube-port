/*
 * dvdreadprio_property_test.c
 *
 * Property-style invariants for DVDReadPrio:
 * - return parity with strict read-window oracle
 * - copied bytes parity/clamping
 * - RAM-backed sdk_state side effects
 * - priority argument does not change semantics
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc_mem.h"
#include "sdk_state.h"

#include "../../../../tests/pbt/dvd/dvd_core_strict_oracle.h"

typedef int32_t s32;
typedef uint32_t u32;

typedef struct {
    volatile s32 state;
} DVDCommandBlock;

typedef struct {
    DVDCommandBlock cb;
    u32 startAddr;
    u32 length;
    s32 entrynum;
} DVDFileInfo;

void gc_dvd_test_reset_files(void);
void gc_dvd_test_set_file(s32 entrynum, const void *data, u32 len);
int DVDFastOpen(s32 entrynum, DVDFileInfo *file);
s32 DVDReadPrio(DVDFileInfo *file, void *addr, s32 len, s32 offset, s32 prio);

static uint32_t g_rng;
static uint64_t g_checks;
static uint64_t g_pass;
static int g_verbose;
static const char *g_op = "FULL";

static uint32_t xorshift32(void) {
    uint32_t x = g_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng = x;
    return x;
}

#define CHECK(cond, fmt, ...) do { \
    g_checks++; \
    if (!(cond)) { \
        printf("FAIL: " fmt "\n", ##__VA_ARGS__); \
        return 0; \
    } \
    g_pass++; \
} while (0)

static int op_is(const char *name) { return strcmp(g_op, name) == 0; }

static int should_run(const char *name) {
    return op_is("FULL") || op_is("MIX") || op_is(name);
}

static int run_seed(uint32_t seed) {
    static uint8_t mem1[0x01800000];
    uint8_t dst_a[8192];
    uint8_t dst_b[8192];
    DVDFileInfo fi;
    uint8_t *src;
    u32 file_len;
    s32 off, len, prio_a, prio_b;
    strict_dvd_read_window_t strict;
    s32 ret_a, ret_b;

    g_rng = seed;

    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();
    gc_dvd_test_reset_files();

    file_len = (xorshift32() % 4096u) + 1u;
    src = (uint8_t *)malloc(file_len);
    if (!src) {
        printf("FAIL: malloc file_len=%u\n", file_len);
        return 0;
    }
    for (u32 i = 0; i < file_len; i++) src[i] = (uint8_t)(xorshift32() & 0xFFu);
    gc_dvd_test_set_file(0, src, file_len);

    memset(&fi, 0, sizeof(fi));
    CHECK(DVDFastOpen(0, &fi) == 1, "DVDFastOpen failed");

    off = (s32)(xorshift32() % 5000u) - 500;
    len = (s32)(xorshift32() % 5000u) - 500;
    prio_a = (s32)(xorshift32() & 0x7FFFFFFFu);
    prio_b = (s32)(xorshift32() & 0x7FFFFFFFu);

    strict = strict_dvd_read_window(file_len, off, len);

    memset(dst_a, 0xAA, sizeof(dst_a));
    memset(dst_b, 0xAA, sizeof(dst_b));

    if (should_run("L0") || should_run("PARITY")) {
        ret_a = DVDReadPrio(&fi, dst_a, len, off, prio_a);
        CHECK(ret_a == strict.prio_ret,
              "L0 return parity ret=%d exp=%d off=%d len=%d", ret_a, strict.prio_ret, off, len);
    }

    if (should_run("L1") || should_run("COPY")) {
        ret_a = DVDReadPrio(&fi, dst_a, len, off, prio_a);
        if (strict.n >= 0) {
            CHECK(memcmp(dst_a, src + off, (size_t)strict.n) == 0,
                  "L1 copied bytes mismatch n=%d off=%d len=%d", strict.n, off, len);
            if ((size_t)strict.n < sizeof(dst_a)) {
                CHECK(dst_a[strict.n] == 0xAA, "L1 write overrun n=%d", strict.n);
            }
        } else {
            CHECK(dst_a[0] == 0xAA, "L1 invalid case should not write");
        }
        (void)ret_a;
    }

    if (should_run("L2") || should_run("STATE")) {
        u32 before_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_READ_CALLS);
        ret_a = DVDReadPrio(&fi, dst_a, len, off, prio_a);
        u32 after_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_READ_CALLS);
        u32 got_len = gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_LAST_READ_LEN);
        u32 got_off = gc_sdk_state_load_u32be(GC_SDK_OFF_DVD_LAST_READ_OFF);
        CHECK(after_calls == before_calls + 1u,
              "L2 read_calls increment before=%u after=%u", before_calls, after_calls);
        CHECK(got_len == (u32)len, "L2 last_read_len got=0x%08X exp=0x%08X", got_len, (u32)len);
        CHECK(got_off == (u32)off, "L2 last_read_off got=0x%08X exp=0x%08X", got_off, (u32)off);
        (void)ret_a;
    }

    if (should_run("L3") || should_run("PRIO")) {
        ret_a = DVDReadPrio(&fi, dst_a, len, off, prio_a);
        ret_b = DVDReadPrio(&fi, dst_b, len, off, prio_b);
        CHECK(ret_a == ret_b, "L3 prio-invariant ret_a=%d ret_b=%d", ret_a, ret_b);
        CHECK(memcmp(dst_a, dst_b, sizeof(dst_a)) == 0, "L3 prio-invariant payload mismatch");
    }

    free(src);
    return 1;
}

int main(int argc, char **argv) {
    uint32_t num_runs = 200;
    uint32_t seed = 0xC0DEC0DEu;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            seed = (uint32_t)strtoul(argv[i] + 7, NULL, 0);
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            num_runs = (uint32_t)strtoul(argv[i] + 11, NULL, 0);
        } else if (strncmp(argv[i], "--op=", 5) == 0) {
            g_op = argv[i] + 5;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            g_verbose = 1;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 2;
        }
    }

    for (uint32_t i = 0; i < num_runs; i++) {
        uint32_t s = seed ^ (0x9E3779B9u * (i + 1u));
        if (!run_seed(s)) {
            printf("FAILED seed=0x%08X op=%s\n", s, g_op);
            return 1;
        }
        if (g_verbose && ((i % 50u) == 0u)) {
            printf("[ok] i=%u seed=0x%08X op=%s\n", i, s, g_op);
        }
    }

    printf("\n=== DVDReadPrio Property Test ===\n");
    printf("Seeds:  %u\n", num_runs);
    printf("Checks: %llu  (pass=%llu fail=%llu)\n",
           (unsigned long long)g_checks,
           (unsigned long long)g_pass,
           (unsigned long long)(g_checks - g_pass));
    printf("RESULT: %llu/%llu PASS\n",
           (unsigned long long)g_pass,
           (unsigned long long)g_checks);
    return 0;
}
