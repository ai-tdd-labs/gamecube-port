/*
 * card_fat_property_test.c --- Property-based parity test for CARD FAT.
 *
 * Oracle:  decomp functions (native u16 array) in card_fat_oracle.h
 * Port:    sdk_port functions (big-endian u16 in gc_mem) in card_fat.c
 *
 * For each seed:
 *   1. Random FAT size (cBlock 64..2048)
 *   2. Random initial state (some blocks pre-allocated)
 *   3. Random mix of AllocBlock + FreeBlock operations
 *   4. After each operation: compare return codes, startBlock, and full FAT
 *
 * Test levels:
 *   L0: __CARDCheckSum leaf (random data, compare checksums)
 *   L1: AllocBlock / FreeBlock individually
 *   L2: Random alloc+free integration
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Oracle (native, self-contained) */
#include "card_fat_oracle.h"

/* Port (big-endian via gc_mem) */
#include "card_fat.h"
#include "gc_mem.h"

/* ── PRNG (xorshift32, same as MTX/OSAlloc tests) ── */
static uint32_t g_rng;

static uint32_t xorshift32(void)
{
    uint32_t x = g_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng = x;
    return x;
}

static uint32_t rand_range(uint32_t lo, uint32_t hi)
{
    return lo + xorshift32() % (hi - lo + 1);
}

/* ── gc_mem backing buffer for port FAT ── */
#define GC_FAT_BASE  0x80100000u
static uint8_t g_gc_ram[PORT_CARD_FAT_BYTES];

/* ── Tracking allocated chains for FreeBlock testing ── */
#define MAX_CHAINS 256
static uint16_t g_chain_starts[MAX_CHAINS];
static int g_num_chains;

/* ── CLI options ── */
static int opt_verbose  = 0;
static int opt_seed     = -1;
static int opt_num_runs = 500;
static const char *opt_op = NULL;

/* ── Statistics ── */
static int g_total_checks = 0;
static int g_total_pass   = 0;
static int g_total_fail   = 0;

/* ── Helpers ── */

static void init_gc_mem(void)
{
    memset(g_gc_ram, 0, sizeof(g_gc_ram));
    gc_mem_set(GC_FAT_BASE, sizeof(g_gc_ram), g_gc_ram);
}

/* Write a u16 value to gc_mem at fat[index] in big-endian */
static void gc_fat_store(uint32_t index, uint16_t val)
{
    uint32_t addr = GC_FAT_BASE + index * 2;
    uint8_t *p = gc_mem_ptr(addr, 2);
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)(val);
}

/* Read a u16 value from gc_mem at fat[index] */
static uint16_t gc_fat_load(uint32_t index)
{
    uint32_t addr = GC_FAT_BASE + index * 2;
    uint8_t *p = gc_mem_ptr(addr, 2);
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

/* Initialize both oracle and port FAT to identical state */
static void init_fat_pair(oracle_CARDControl *oc, port_CARDControl *pc,
                          uint16_t *oracle_fat, uint16_t cBlock)
{
    uint16_t i;
    uint16_t free_count;

    /* Clear */
    memset(oracle_fat, 0, ORACLE_CARD_FAT_BYTES);
    init_gc_mem();

    /* Set data blocks as free (0x0000) --- they already are from memset */
    /* Count free blocks: [5..cBlock-1] */
    free_count = cBlock - ORACLE_CARD_NUM_SYSTEM_BLOCK;

    /* Set metadata */
    oracle_fat[ORACLE_CARD_FAT_CHECKCODE]  = 0;   /* update counter */
    oracle_fat[ORACLE_CARD_FAT_FREEBLOCKS] = free_count;
    oracle_fat[ORACLE_CARD_FAT_LASTSLOT]   = (uint16_t)(cBlock - 1); /* hint at last block */

    /* Compute initial checksum */
    oracle_CARDCheckSum(&oracle_fat[ORACLE_CARD_FAT_CHECKCODE], 0x1FFC,
                        &oracle_fat[ORACLE_CARD_FAT_CHECKSUM],
                        &oracle_fat[ORACLE_CARD_FAT_CHECKSUMINV]);

    /* Copy oracle FAT to port (big-endian) */
    for (i = 0; i < ORACLE_CARD_FAT_ENTRIES; i++) {
        gc_fat_store(i, oracle_fat[i]);
    }

    /* Set up control structs */
    oc->attached   = 1;
    oc->cBlock     = cBlock;
    oc->startBlock = 0xFFFF;
    oc->currentFat = oracle_fat;

    pc->attached   = 1;
    pc->cBlock     = cBlock;
    pc->startBlock = 0xFFFF;
    pc->fat_addr   = GC_FAT_BASE;

    /* Reset chain tracking */
    g_num_chains = 0;
}

/* Compare oracle FAT (native u16) with port FAT (big-endian gc_mem).
 * Returns number of mismatches. */
static int compare_fat(const uint16_t *oracle_fat, uint16_t cBlock,
                       const char *op_name, uint32_t seed)
{
    int mismatches = 0;
    uint16_t i;

    /* Compare all entries up to cBlock (the usable range).
     * Also compare metadata entries [0..4]. */
    uint16_t limit = cBlock;
    if (limit < 5) limit = 5;

    for (i = 0; i < limit; i++) {
        uint16_t port_val = gc_fat_load(i);
        if (oracle_fat[i] != port_val) {
            if (mismatches < 5) {
                fprintf(stderr,
                    "  MISMATCH fat[%u]: oracle=0x%04X port=0x%04X "
                    "(op=%s seed=%u)\n",
                    (unsigned)i, (unsigned)oracle_fat[i],
                    (unsigned)port_val, op_name, seed);
            }
            mismatches++;
        }
    }
    return mismatches;
}

/* ── L0: CheckSum leaf test ── */

static int test_checksum(uint32_t seed)
{
    int pass = 0, fail = 0;
    uint16_t native_buf[256];
    uint16_t ck_oracle, cki_oracle, ck_port, cki_port;
    int i, length;

    /* Fill with random u16 values */
    for (i = 0; i < 256; i++) {
        native_buf[i] = (uint16_t)(xorshift32() & 0xFFFF);
    }

    /* Choose random length (even, in bytes) */
    length = (int)(rand_range(4, 512) & ~1u);
    if (length > 512) length = 512;

    /* Oracle: native u16 array */
    oracle_CARDCheckSum(native_buf, length, &ck_oracle, &cki_oracle);

    /* Port: copy to gc_mem in big-endian, then checksum */
    init_gc_mem();
    for (i = 0; i < length / 2; i++) {
        gc_fat_store((uint32_t)i, native_buf[i]);
    }
    port_CARDCheckSum(GC_FAT_BASE, length, &ck_port, &cki_port);

    g_total_checks++;
    if (ck_oracle == ck_port && cki_oracle == cki_port) {
        pass++;
        g_total_pass++;
    } else {
        fail++;
        g_total_fail++;
        fprintf(stderr,
            "  FAIL L0-CheckSum seed=%u len=%d: "
            "oracle=(0x%04X,0x%04X) port=(0x%04X,0x%04X)\n",
            seed, length,
            (unsigned)ck_oracle, (unsigned)cki_oracle,
            (unsigned)ck_port, (unsigned)cki_port);
    }

    if (opt_verbose && pass > 0) {
        printf("  L0-CheckSum: len=%d ck=0x%04X cki=0x%04X OK\n",
               length, (unsigned)ck_oracle, (unsigned)cki_oracle);
    }

    return fail;
}

/* ── L1: AllocBlock / FreeBlock individual tests ── */

static int test_alloc_block(oracle_CARDControl *oc, port_CARDControl *pc,
                            uint16_t *oracle_fat, uint32_t seed)
{
    uint32_t free_count = oracle_fat[ORACLE_CARD_FAT_FREEBLOCKS];
    uint32_t max_alloc;
    uint32_t n_blocks;
    int32_t rc_oracle, rc_port;

    if (free_count == 0) return 0;

    /* Allocate 1..min(free_count, 16) blocks */
    max_alloc = free_count;
    if (max_alloc > 16) max_alloc = 16;
    n_blocks = rand_range(1, max_alloc);

    rc_oracle = oracle_CARDAllocBlock(oc, n_blocks);
    rc_port   = port_CARDAllocBlock(pc, n_blocks);

    g_total_checks++;
    if (rc_oracle != rc_port) {
        g_total_fail++;
        fprintf(stderr,
            "  FAIL L1-Alloc seed=%u n=%u: rc oracle=%d port=%d\n",
            seed, n_blocks, rc_oracle, rc_port);
        return 1;
    }

    if (oc->startBlock != pc->startBlock) {
        g_total_fail++;
        fprintf(stderr,
            "  FAIL L1-Alloc seed=%u n=%u: startBlock oracle=%u port=%u\n",
            seed, n_blocks, (unsigned)oc->startBlock, (unsigned)pc->startBlock);
        return 1;
    }

    int mismatches = compare_fat(oracle_fat, oc->cBlock, "L1-Alloc", seed);
    if (mismatches > 0) {
        g_total_fail++;
        return 1;
    }

    /* Track chain for later freeing */
    if (rc_oracle == ORACLE_CARD_RESULT_READY && g_num_chains < MAX_CHAINS) {
        g_chain_starts[g_num_chains++] = oc->startBlock;
    }

    g_total_pass++;
    if (opt_verbose) {
        printf("  L1-Alloc: n=%u start=%u rc=%d OK\n",
               n_blocks, (unsigned)oc->startBlock, rc_oracle);
    }
    return 0;
}

static int test_free_block(oracle_CARDControl *oc, port_CARDControl *pc,
                           uint16_t *oracle_fat, uint32_t seed)
{
    int32_t rc_oracle, rc_port;
    uint16_t chain_start;
    int chain_idx;

    if (g_num_chains == 0) return 0;

    /* Pick a random chain to free */
    chain_idx = (int)(xorshift32() % (uint32_t)g_num_chains);
    chain_start = g_chain_starts[chain_idx];

    /* Remove from tracking (swap with last) */
    g_chain_starts[chain_idx] = g_chain_starts[g_num_chains - 1];
    g_num_chains--;

    rc_oracle = oracle_CARDFreeBlock(oc, chain_start);
    rc_port   = port_CARDFreeBlock(pc, chain_start);

    g_total_checks++;
    if (rc_oracle != rc_port) {
        g_total_fail++;
        fprintf(stderr,
            "  FAIL L1-Free seed=%u start=%u: rc oracle=%d port=%d\n",
            seed, (unsigned)chain_start, rc_oracle, rc_port);
        return 1;
    }

    int mismatches = compare_fat(oracle_fat, oc->cBlock, "L1-Free", seed);
    if (mismatches > 0) {
        g_total_fail++;
        return 1;
    }

    g_total_pass++;
    if (opt_verbose) {
        printf("  L1-Free: start=%u rc=%d OK\n",
               (unsigned)chain_start, rc_oracle);
    }
    return 0;
}

/* ── Run one seed ── */

static int run_seed(uint32_t seed)
{
    oracle_CARDControl oc;
    port_CARDControl pc;
    uint16_t oracle_fat[ORACLE_CARD_FAT_ENTRIES];
    uint16_t cBlock;
    int fail = 0;
    int i, n_ops;

    g_rng = seed;

    /* Random card size: 64..2048 total blocks */
    cBlock = (uint16_t)rand_range(64, 2048);

    init_fat_pair(&oc, &pc, oracle_fat, cBlock);

    if (opt_verbose) {
        printf("seed=%u cBlock=%u freeBlocks=%u\n",
               seed, (unsigned)cBlock,
               (unsigned)oracle_fat[ORACLE_CARD_FAT_FREEBLOCKS]);
    }

    /* L0: CheckSum test */
    if (!opt_op || strstr("CHECKSUM", opt_op) || strstr("L0", opt_op)) {
        fail += test_checksum(seed);
    }

    /* Re-initialize FAT pair: L0 trashes gc_mem with random test data */
    init_fat_pair(&oc, &pc, oracle_fat, cBlock);

    /* L1+L2: Mixed alloc/free operations */
    if (!opt_op || strstr("ALLOC", opt_op) || strstr("FREE", opt_op) ||
        strstr("L1", opt_op) || strstr("L2", opt_op) || strstr("FAT", opt_op)) {

        n_ops = (int)rand_range(10, 50);
        for (i = 0; i < n_ops && fail == 0; i++) {
            uint32_t r = xorshift32() % 100;

            if (r < 60 || g_num_chains == 0) {
                /* 60% alloc (or forced alloc if no chains to free) */
                fail += test_alloc_block(&oc, &pc, oracle_fat, seed);
            } else {
                /* 40% free */
                fail += test_free_block(&oc, &pc, oracle_fat, seed);
            }
        }
    }

    return fail;
}

/* ── CLI arg parsing ── */

static void parse_args(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            opt_seed = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            opt_num_runs = atoi(argv[i] + 11);
        } else if (strncmp(argv[i], "--op=", 5) == 0) {
            opt_op = argv[i] + 5;
        } else if (strcmp(argv[i], "-v") == 0) {
            opt_verbose = 1;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            fprintf(stderr,
                "Usage: card_fat_property_test [--seed=N] [--num-runs=N] "
                "[--op=NAME] [-v]\n");
            exit(1);
        }
    }
}

/* ── Main ── */

int main(int argc, char **argv)
{
    int total_seeds, fails;
    uint32_t s;

    parse_args(argc, argv);

    printf("=== CARD FAT Property Test ===\n");

    if (opt_seed >= 0) {
        /* Single seed mode */
        total_seeds = 1;
        fails = run_seed((uint32_t)opt_seed);
    } else {
        /* Multi-seed mode */
        total_seeds = opt_num_runs;
        fails = 0;
        for (s = 1; s <= (uint32_t)opt_num_runs; s++) {
            fails += run_seed(s);
        }
    }

    printf("\n--- Summary ---\n");
    printf("Seeds:  %d\n", total_seeds);
    printf("Checks: %d  (pass=%d  fail=%d)\n",
           g_total_checks, g_total_pass, g_total_fail);

    if (g_total_fail == 0) {
        printf("\nRESULT: %d/%d PASS\n", g_total_pass, g_total_checks);
        return 0;
    } else {
        printf("\nRESULT: FAIL (%d failures)\n", g_total_fail);
        return 1;
    }
}
