/*
 * card_unlock_property_test.c --- Property-based parity test for CARD unlock crypto.
 *
 * Oracle:  exact copy of decomp (card_unlock_oracle.h)
 * Port:    sdk_port/card/card_unlock.c
 *
 * Test levels:
 *   L0 — bitrev: oracle vs port parity + involution (bitrev(bitrev(x)) == x)
 *   L1 — exnor_1st: oracle vs port parity, shift=0 identity
 *   L2 — exnor: oracle vs port parity, shift=0 identity
 *   L3 — CARDRand/CARDSrand: oracle vs port sequence parity
 *   L4 — Cross-checks: exnor used in __CARDUnlock scramble pattern
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Oracle (exact decomp copy) */
#include "card_unlock_oracle.h"

/* Port (sdk_port) */
#include "card_unlock.h"

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

/* ═══════════════════════════════════════════════════════════════════
 * L0: bitrev parity + involution
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L0_bitrev(void)
{
    uint32_t val = xorshift32();
    uint32_t o_result, p_result;

    /* Oracle vs port parity */
    o_result = oracle_bitrev(val);
    p_result = port_bitrev(val);
    CHECK(o_result == p_result,
          "L0 bitrev parity: val=0x%08X oracle=0x%08X port=0x%08X",
          val, o_result, p_result);

    /* Involution: bitrev(bitrev(x)) == x */
    CHECK(oracle_bitrev(o_result) == val,
          "L0 bitrev involution: bitrev(bitrev(0x%08X)) != 0x%08X",
          val, val);

    /* Known values */
    CHECK(oracle_bitrev(0) == 0,
          "L0 bitrev(0) should be 0");
    CHECK(oracle_bitrev(0xFFFFFFFF) == 0xFFFFFFFF,
          "L0 bitrev(0xFFFFFFFF) should be 0xFFFFFFFF");
    CHECK(oracle_bitrev(1) == port_bitrev(1),
          "L0 bitrev(1) parity");
}

/* ═══════════════════════════════════════════════════════════════════
 * L1: exnor_1st parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L1_exnor_1st(void)
{
    uint32_t val = xorshift32();
    uint32_t shift = xorshift32() % 64; /* 0..63 shifts */
    uint32_t o_result, p_result;

    /* Oracle vs port parity */
    o_result = oracle_exnor_1st(val, shift);
    p_result = port_exnor_1st(val, shift);
    CHECK(o_result == p_result,
          "L1 exnor_1st parity: val=0x%08X shift=%u oracle=0x%08X port=0x%08X",
          val, shift, o_result, p_result);

    /* Shift=0 identity */
    CHECK(oracle_exnor_1st(val, 0) == val,
          "L1 exnor_1st shift=0 identity: val=0x%08X", val);

    /* Single shift parity */
    o_result = oracle_exnor_1st(val, 1);
    p_result = port_exnor_1st(val, 1);
    CHECK(o_result == p_result,
          "L1 exnor_1st shift=1 parity: val=0x%08X", val);
}

/* ═══════════════════════════════════════════════════════════════════
 * L2: exnor parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L2_exnor(void)
{
    uint32_t val = xorshift32();
    uint32_t shift = xorshift32() % 64;
    uint32_t o_result, p_result;

    /* Oracle vs port parity */
    o_result = oracle_exnor(val, shift);
    p_result = port_exnor(val, shift);
    CHECK(o_result == p_result,
          "L2 exnor parity: val=0x%08X shift=%u oracle=0x%08X port=0x%08X",
          val, shift, o_result, p_result);

    /* Shift=0 identity */
    CHECK(oracle_exnor(val, 0) == val,
          "L2 exnor shift=0 identity: val=0x%08X", val);

    /* Single shift parity */
    o_result = oracle_exnor(val, 1);
    p_result = port_exnor(val, 1);
    CHECK(o_result == p_result,
          "L2 exnor shift=1 parity: val=0x%08X", val);
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: CARDRand/CARDSrand sequence parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L3_cardrand(void)
{
    uint32_t seed = xorshift32();
    int seq_len = 10;
    int i;

    oracle_CARDSrand(seed);
    port_CARDSrand(seed);

    for (i = 0; i < seq_len; i++) {
        int o_val = oracle_CARDRand();
        int p_val = port_CARDRand();
        CHECK(o_val == p_val,
              "L3 CARDRand[%d] seed=0x%08X: oracle=%d port=%d",
              i, seed, o_val, p_val);
    }

    /* Range check: CARDRand returns 0..32767 */
    oracle_CARDSrand(seed);
    for (i = 0; i < 100; i++) {
        int val = oracle_CARDRand();
        CHECK(val >= 0 && val < 32768,
              "L3 CARDRand range: val=%d", val);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: Cross-checks (scramble pattern from __CARDUnlock)
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L4_scramble_pattern(void)
{
    /* Simulate the scramble sequence from __CARDUnlock lines 237-241:
     * wk = exnor_1st(init_val, rshift);
     * wk1 = ~(wk ^ (wk >> 7) ^ (wk >> 15) ^ (wk >> 23));
     * scramble = (wk | ((wk1 << 31) & 0x80000000));
     * scramble = bitrev(scramble);
     */
    uint32_t init_val = xorshift32() & 0xFFFFF000u; /* 12-bit aligned like real code */
    uint32_t rshift = (xorshift32() % 256 + 1) * 8 + 1; /* like dummy*8+1 */

    uint32_t o_wk = oracle_exnor_1st(init_val, rshift);
    uint32_t p_wk = port_exnor_1st(init_val, rshift);
    CHECK(o_wk == p_wk, "L4 exnor_1st parity in scramble");

    uint32_t o_wk1 = ~(o_wk ^ (o_wk >> 7) ^ (o_wk >> 15) ^ (o_wk >> 23));
    uint32_t p_wk1 = ~(p_wk ^ (p_wk >> 7) ^ (p_wk >> 15) ^ (p_wk >> 23));

    uint32_t o_scramble = (o_wk | ((o_wk1 << 31) & 0x80000000));
    uint32_t p_scramble = (p_wk | ((p_wk1 << 31) & 0x80000000));
    CHECK(o_scramble == p_scramble, "L4 scramble before bitrev");

    o_scramble = oracle_bitrev(o_scramble);
    p_scramble = port_bitrev(p_scramble);
    CHECK(o_scramble == p_scramble,
          "L4 scramble after bitrev: oracle=0x%08X port=0x%08X",
          o_scramble, p_scramble);

    /* Now simulate the exnor left-shift chain (lines 254-257):
     * rshift = 32;
     * wk = exnor(scramble, rshift);
     * wk1 = ~(wk ^ (wk << 7) ^ (wk << 15) ^ (wk << 23));
     * scramble = (wk | ((wk1 >> 31) & 0x00000001));
     */
    o_wk = oracle_exnor(o_scramble, 32);
    p_wk = port_exnor(p_scramble, 32);
    CHECK(o_wk == p_wk, "L4 exnor chain step");

    o_wk1 = ~(o_wk ^ (o_wk << 7) ^ (o_wk << 15) ^ (o_wk << 23));
    p_wk1 = ~(p_wk ^ (p_wk << 7) ^ (p_wk << 15) ^ (p_wk << 23));

    o_scramble = (o_wk | ((o_wk1 >> 31) & 0x00000001));
    p_scramble = (p_wk | ((p_wk1 >> 31) & 0x00000001));
    CHECK(o_scramble == p_scramble,
          "L4 scramble chain: oracle=0x%08X port=0x%08X",
          o_scramble, p_scramble);
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

    printf("card_unlock_property_test: seed=%u num_runs=%d\n", seed, num_runs);

    for (i = 0; i < num_runs; i++) {
        g_rng = seed + (uint32_t)i;

        test_L0_bitrev();
        test_L1_exnor_1st();
        test_L2_exnor();
        test_L3_cardrand();
        test_L4_scramble_pattern();
    }

    printf("\ncard_unlock_property_test: %d/%d PASS", g_pass, g_pass + g_fail);
    if (g_fail) printf(", %d FAIL", g_fail);
    printf("\n");

    return g_fail ? 1 : 0;
}
