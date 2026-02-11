/*
 * dvdqueue_property_test.c — Property-based parity test for dvdqueue.c
 *
 * Oracle: exact copy of decomp dvdqueue.c (pointer-based, native structs)
 * Port:   reimplemented with gc_mem big-endian access (u32 GC addresses)
 *
 * Levels:
 *   L0 — Push/Pop parity: push random blocks, pop in priority order, compare
 *   L1 — Dequeue parity: push blocks, dequeue random ones, verify
 *   L2 — IsBlockInWaitingQueue parity: query membership after random ops
 *   L3 — Properties: FIFO within same priority, highest prio (0) pops first
 *   L4 — Full integration: random mix of all operations
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
 * ORACLE — exact copy of decomp dvdqueue.c (native pointers)
 * ═══════════════════════════════════════════════════════════════════ */

#define ORACLE_MAX_QUEUES 4
#define ORACLE_MAX_BLOCKS 32

typedef struct oracle_DVDCommandBlock oracle_DVDCommandBlock;
struct oracle_DVDCommandBlock {
    oracle_DVDCommandBlock *next;
    oracle_DVDCommandBlock *prev;
    int id; /* for identification in tests */
};

typedef struct {
    oracle_DVDCommandBlock *next;
    oracle_DVDCommandBlock *prev;
} oracle_DVDQueue;

static oracle_DVDQueue        oracle_WaitingQueue[ORACLE_MAX_QUEUES];
static oracle_DVDCommandBlock oracle_BlockPool[ORACLE_MAX_BLOCKS];

static void oracle_DVDClearWaitingQueue(void) {
    uint32_t i;
    for (i = 0; i < ORACLE_MAX_QUEUES; i++) {
        oracle_DVDCommandBlock *q = (oracle_DVDCommandBlock *)&oracle_WaitingQueue[i];
        q->next = q;
        q->prev = q;
    }
}

static int oracle_DVDPushWaitingQueue(int32_t prio, oracle_DVDCommandBlock *block) {
    oracle_DVDCommandBlock *q = (oracle_DVDCommandBlock *)&oracle_WaitingQueue[prio];
    q->prev->next = block;
    block->prev = q->prev;
    block->next = q;
    q->prev = block;
    return 1;
}

static oracle_DVDCommandBlock *oracle_PopWaitingQueuePrio(int32_t prio) {
    oracle_DVDCommandBlock *q = (oracle_DVDCommandBlock *)&oracle_WaitingQueue[prio];
    oracle_DVDCommandBlock *tmp = q->next;
    q->next = tmp->next;
    tmp->next->prev = q;
    tmp->next = NULL;
    tmp->prev = NULL;
    return tmp;
}

static oracle_DVDCommandBlock *oracle_DVDPopWaitingQueue(void) {
    uint32_t i;
    for (i = 0; i < ORACLE_MAX_QUEUES; i++) {
        oracle_DVDCommandBlock *q = (oracle_DVDCommandBlock *)&oracle_WaitingQueue[i];
        if (q->next != q) {
            return oracle_PopWaitingQueuePrio((int32_t)i);
        }
    }
    return NULL;
}

static int oracle_DVDCheckWaitingQueue(void) {
    uint32_t i;
    for (i = 0; i < ORACLE_MAX_QUEUES; i++) {
        oracle_DVDCommandBlock *q = (oracle_DVDCommandBlock *)&oracle_WaitingQueue[i];
        if (q->next != q) return 1;
    }
    return 0;
}

static int oracle_DVDDequeueWaitingQueue(oracle_DVDCommandBlock *block) {
    oracle_DVDCommandBlock *prev = block->prev;
    oracle_DVDCommandBlock *next = block->next;
    if (prev == NULL || next == NULL) return 0;
    prev->next = next;
    next->prev = prev;
    return 1;
}

static int oracle_DVDIsBlockInWaitingQueue(oracle_DVDCommandBlock *block) {
    uint32_t i;
    for (i = 0; i < ORACLE_MAX_QUEUES; i++) {
        oracle_DVDCommandBlock *start = (oracle_DVDCommandBlock *)&oracle_WaitingQueue[i];
        if (start->next != start) {
            oracle_DVDCommandBlock *q;
            for (q = start->next; q != start; q = q->next) {
                if (q == block) return 1;
            }
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * PORT — gc_mem big-endian reimplementation
 * ═══════════════════════════════════════════════════════════════════ */

/*
 * Memory layout (big-endian u32 fields):
 *
 * DVDQueue:        next(4) prev(4) = 8 bytes
 * DVDCommandBlock: next(4) prev(4) id(4) = 12 bytes
 *
 * Layout in gc_mem:
 *   offset 0:    DVDQueue[4]          = 4 * 8 = 32 bytes
 *   offset 32:   DVDCommandBlock[32]  = 32 * 12 = 384 bytes
 *   Total: 416 bytes
 */

#define PORT_QUEUE_SIZE    8
#define PORT_BLOCK_SIZE   12
#define PORT_BLOCK_NEXT    0
#define PORT_BLOCK_PREV    4
#define PORT_BLOCK_ID      8
#define PORT_MAX_QUEUES    4
#define PORT_MAX_BLOCKS   32

/* Base offset must be > 0 so no valid address equals 0 (our NULL sentinel) */
#define PORT_BASE_OFFSET    4
#define PORT_QUEUES_OFFSET  PORT_BASE_OFFSET
#define PORT_BLOCKS_OFFSET  (PORT_QUEUES_OFFSET + PORT_MAX_QUEUES * PORT_QUEUE_SIZE)
#define PORT_TOTAL_SIZE     (PORT_BLOCKS_OFFSET + PORT_MAX_BLOCKS * PORT_BLOCK_SIZE)

static uint8_t port_mem[PORT_TOTAL_SIZE];

/* Big-endian read/write */
static uint32_t be_get32(uint8_t *base, int off) {
    uint8_t *p = base + off;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static void be_set32(uint8_t *base, int off, uint32_t val) {
    uint8_t *p = base + off;
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8);
    p[3] = (uint8_t)val;
}

/* GC address helpers — we use offsets within port_mem as "GC addresses" */
#define PORT_QUEUE_ADDR(i)  (PORT_QUEUES_OFFSET + (i) * PORT_QUEUE_SIZE)
#define PORT_BLOCK_ADDR(i)  (PORT_BLOCKS_OFFSET + (i) * PORT_BLOCK_SIZE)

/* Access helpers for the port memory */
#define qr32(addr, field)        be_get32(port_mem, (addr) + (field))
#define qw32(addr, field, val)   be_set32(port_mem, (addr) + (field), (val))

static void port_DVDClearWaitingQueue(void) {
    uint32_t i;
    for (i = 0; i < PORT_MAX_QUEUES; i++) {
        uint32_t q = PORT_QUEUE_ADDR(i);
        qw32(q, PORT_BLOCK_NEXT, q);
        qw32(q, PORT_BLOCK_PREV, q);
    }
}

static int port_DVDPushWaitingQueue(int32_t prio, uint32_t block_addr) {
    uint32_t q = PORT_QUEUE_ADDR(prio);
    uint32_t q_prev = qr32(q, PORT_BLOCK_PREV);

    /* q->prev->next = block */
    qw32(q_prev, PORT_BLOCK_NEXT, block_addr);
    /* block->prev = q->prev */
    qw32(block_addr, PORT_BLOCK_PREV, q_prev);
    /* block->next = q */
    qw32(block_addr, PORT_BLOCK_NEXT, q);
    /* q->prev = block */
    qw32(q, PORT_BLOCK_PREV, block_addr);

    return 1;
}

static uint32_t port_PopWaitingQueuePrio(int32_t prio) {
    uint32_t q = PORT_QUEUE_ADDR(prio);
    uint32_t tmp = qr32(q, PORT_BLOCK_NEXT);
    uint32_t tmp_next = qr32(tmp, PORT_BLOCK_NEXT);

    /* q->next = tmp->next */
    qw32(q, PORT_BLOCK_NEXT, tmp_next);
    /* tmp->next->prev = q */
    qw32(tmp_next, PORT_BLOCK_PREV, q);

    /* tmp->next = NULL, tmp->prev = NULL */
    qw32(tmp, PORT_BLOCK_NEXT, 0);
    qw32(tmp, PORT_BLOCK_PREV, 0);

    return tmp;
}

static uint32_t port_DVDPopWaitingQueue(void) {
    uint32_t i;
    for (i = 0; i < PORT_MAX_QUEUES; i++) {
        uint32_t q = PORT_QUEUE_ADDR(i);
        if (qr32(q, PORT_BLOCK_NEXT) != q) {
            return port_PopWaitingQueuePrio((int32_t)i);
        }
    }
    return 0; /* NULL */
}

static int port_DVDCheckWaitingQueue(void) {
    uint32_t i;
    for (i = 0; i < PORT_MAX_QUEUES; i++) {
        uint32_t q = PORT_QUEUE_ADDR(i);
        if (qr32(q, PORT_BLOCK_NEXT) != q) return 1;
    }
    return 0;
}

static int port_DVDDequeueWaitingQueue(uint32_t block_addr) {
    uint32_t prev = qr32(block_addr, PORT_BLOCK_PREV);
    uint32_t next = qr32(block_addr, PORT_BLOCK_NEXT);

    if (prev == 0 || next == 0) return 0;

    /* prev->next = next */
    qw32(prev, PORT_BLOCK_NEXT, next);
    /* next->prev = prev */
    qw32(next, PORT_BLOCK_PREV, prev);

    return 1;
}

static int port_DVDIsBlockInWaitingQueue(uint32_t block_addr) {
    uint32_t i;
    for (i = 0; i < PORT_MAX_QUEUES; i++) {
        uint32_t start = PORT_QUEUE_ADDR(i);
        if (qr32(start, PORT_BLOCK_NEXT) != start) {
            uint32_t q;
            for (q = qr32(start, PORT_BLOCK_NEXT); q != start; q = qr32(q, PORT_BLOCK_NEXT)) {
                if (q == block_addr) return 1;
            }
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * Test infrastructure
 * ═══════════════════════════════════════════════════════════════════ */

/* Track which blocks are in which queue (or none) for test logic */
#define IN_NONE   (-1)
static int block_queue[ORACLE_MAX_BLOCKS]; /* which prio queue, or IN_NONE */

static void init_both(void) {
    int i;
    oracle_DVDClearWaitingQueue();
    memset(port_mem, 0, sizeof(port_mem));
    port_DVDClearWaitingQueue();

    for (i = 0; i < ORACLE_MAX_BLOCKS; i++) {
        oracle_BlockPool[i].next = NULL;
        oracle_BlockPool[i].prev = NULL;
        oracle_BlockPool[i].id   = i;
        qw32(PORT_BLOCK_ADDR(i), PORT_BLOCK_NEXT, 0);
        qw32(PORT_BLOCK_ADDR(i), PORT_BLOCK_PREV, 0);
        qw32(PORT_BLOCK_ADDR(i), PORT_BLOCK_ID,   (uint32_t)i);
        block_queue[i] = IN_NONE;
    }
}

/* Map oracle block pointer to block index */
static int oracle_block_id(oracle_DVDCommandBlock *b) {
    if (!b) return -1;
    return b->id;
}

/* Map port block address to block index */
static int port_block_id(uint32_t addr) {
    if (addr == 0) return -1;
    return (int)((addr - PORT_BLOCKS_OFFSET) / PORT_BLOCK_SIZE);
}

/* ═══════════════════════════════════════════════════════════════════
 * L0 — Push/Pop parity
 * ═══════════════════════════════════════════════════════════════════ */
static int test_push_pop(uint32_t seed) {
    int num_ops, i;
    g_rng = seed;
    init_both();

    num_ops = 5 + (xorshift32() % 20);

    /* Push random blocks to random priorities */
    for (i = 0; i < num_ops && i < ORACLE_MAX_BLOCKS; i++) {
        int32_t prio = (int32_t)(xorshift32() % ORACLE_MAX_QUEUES);
        oracle_DVDPushWaitingQueue(prio, &oracle_BlockPool[i]);
        port_DVDPushWaitingQueue(prio, PORT_BLOCK_ADDR(i));
        block_queue[i] = prio;
    }

    /* Check parity */
    {
        int oc = oracle_DVDCheckWaitingQueue();
        int pc = port_DVDCheckWaitingQueue();
        CHECK(oc == pc, "CheckWaitingQueue: oracle=%d port=%d", oc, pc);
    }

    /* Pop all and compare order */
    for (;;) {
        oracle_DVDCommandBlock *ob = oracle_DVDPopWaitingQueue();
        uint32_t pb = port_DVDPopWaitingQueue();
        int oid = oracle_block_id(ob);
        int pid = port_block_id(pb);
        CHECK(oid == pid, "PopWaitingQueue: oracle=%d port=%d", oid, pid);
        if (oid == -1) break;
    }

    /* Both should be empty now */
    CHECK(!oracle_DVDCheckWaitingQueue(), "oracle not empty after drain");
    CHECK(!port_DVDCheckWaitingQueue(), "port not empty after drain");

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L1 — Dequeue parity
 * ═══════════════════════════════════════════════════════════════════ */
static int test_dequeue(uint32_t seed) {
    int num_push, num_deq, i, enqueued;
    g_rng = seed;
    init_both();

    num_push = 5 + (xorshift32() % 16);
    if (num_push > ORACLE_MAX_BLOCKS) num_push = ORACLE_MAX_BLOCKS;

    /* Push blocks */
    for (i = 0; i < num_push; i++) {
        int32_t prio = (int32_t)(xorshift32() % ORACLE_MAX_QUEUES);
        oracle_DVDPushWaitingQueue(prio, &oracle_BlockPool[i]);
        port_DVDPushWaitingQueue(prio, PORT_BLOCK_ADDR(i));
        block_queue[i] = (int)prio;
    }

    /* Count enqueued blocks */
    enqueued = num_push;

    /* Dequeue random blocks — only ones currently in queue */
    num_deq = 1 + (xorshift32() % (num_push > 1 ? num_push : 1));
    for (i = 0; i < num_deq && enqueued > 0; i++) {
        int idx = (int)(xorshift32() % (uint32_t)num_push);
        if (block_queue[idx] == IN_NONE) continue; /* skip already-dequeued */
        {
            int or_result = oracle_DVDDequeueWaitingQueue(&oracle_BlockPool[idx]);
            int pr_result = port_DVDDequeueWaitingQueue(PORT_BLOCK_ADDR(idx));
            CHECK(or_result == pr_result,
                  "Dequeue block %d: oracle=%d port=%d", idx, or_result, pr_result);
            if (or_result) {
                block_queue[idx] = IN_NONE;
                enqueued--;
            }
        }
    }

    /* Drain remaining and compare */
    for (;;) {
        oracle_DVDCommandBlock *ob = oracle_DVDPopWaitingQueue();
        uint32_t pb = port_DVDPopWaitingQueue();
        int oid = oracle_block_id(ob);
        int pid = port_block_id(pb);
        CHECK(oid == pid, "Pop after dequeue: oracle=%d port=%d", oid, pid);
        if (oid == -1) break;
    }

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2 — IsBlockInWaitingQueue parity
 * ═══════════════════════════════════════════════════════════════════ */
static int test_is_in_queue(uint32_t seed) {
    int num_push, i;
    g_rng = seed;
    init_both();

    num_push = 3 + (xorshift32() % 20);
    if (num_push > ORACLE_MAX_BLOCKS) num_push = ORACLE_MAX_BLOCKS;

    /* Push some blocks */
    for (i = 0; i < num_push; i++) {
        int32_t prio = (int32_t)(xorshift32() % ORACLE_MAX_QUEUES);
        oracle_DVDPushWaitingQueue(prio, &oracle_BlockPool[i]);
        port_DVDPushWaitingQueue(prio, PORT_BLOCK_ADDR(i));
        block_queue[i] = (int)prio;
    }

    /* Dequeue some randomly — only ones currently in queue */
    for (i = 0; i < num_push / 2; i++) {
        int idx = (int)(xorshift32() % (uint32_t)num_push);
        if (block_queue[idx] == IN_NONE) continue;
        oracle_DVDDequeueWaitingQueue(&oracle_BlockPool[idx]);
        port_DVDDequeueWaitingQueue(PORT_BLOCK_ADDR(idx));
        block_queue[idx] = IN_NONE;
    }

    /* Check membership for ALL blocks (pushed and not-pushed) */
    for (i = 0; i < ORACLE_MAX_BLOCKS; i++) {
        int ov = oracle_DVDIsBlockInWaitingQueue(&oracle_BlockPool[i]);
        int pv = port_DVDIsBlockInWaitingQueue(PORT_BLOCK_ADDR(i));
        CHECK(ov == pv, "IsBlockInQueue(%d): oracle=%d port=%d", i, ov, pv);
    }

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3 — Properties: FIFO within priority, highest prio pops first
 * ═══════════════════════════════════════════════════════════════════ */
static int test_properties(uint32_t seed) {
    int i, n;
    int push_order[ORACLE_MAX_BLOCKS];
    int push_prio[ORACLE_MAX_BLOCKS];
    g_rng = seed;
    init_both();

    n = 8 + (xorshift32() % 16);
    if (n > ORACLE_MAX_BLOCKS) n = ORACLE_MAX_BLOCKS;

    /* Push blocks in order, recording prio */
    for (i = 0; i < n; i++) {
        int32_t prio = (int32_t)(xorshift32() % ORACLE_MAX_QUEUES);
        oracle_DVDPushWaitingQueue(prio, &oracle_BlockPool[i]);
        port_DVDPushWaitingQueue(prio, PORT_BLOCK_ADDR(i));
        push_order[i] = i;
        push_prio[i] = (int)prio;
        block_queue[i] = (int)prio;
    }

    /* Pop all — verify priority ordering and FIFO within priority */
    {
        int last_prio = -1;
        int last_id_per_prio[ORACLE_MAX_QUEUES];
        for (i = 0; i < ORACLE_MAX_QUEUES; i++) last_id_per_prio[i] = -1;

        for (i = 0; i < n; i++) {
            oracle_DVDCommandBlock *ob = oracle_DVDPopWaitingQueue();
            uint32_t pb = port_DVDPopWaitingQueue();
            int oid = oracle_block_id(ob);
            int pid = port_block_id(pb);

            CHECK(oid == pid, "Property pop: oracle=%d port=%d", oid, pid);
            CHECK(oid >= 0, "Expected non-null pop at step %d", i);

            {
                int this_prio = push_prio[oid];

                /* Property 1: pops in non-decreasing priority order */
                CHECK(this_prio >= last_prio,
                      "Priority order: got prio %d after %d", this_prio, last_prio);
                last_prio = this_prio;

                /* Property 2: FIFO within same priority */
                if (last_id_per_prio[this_prio] >= 0) {
                    CHECK(oid > last_id_per_prio[this_prio],
                          "FIFO within prio %d: id %d after %d",
                          this_prio, oid, last_id_per_prio[this_prio]);
                }
                last_id_per_prio[this_prio] = oid;
            }
        }
    }

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4 — Full integration: random mix of all operations
 * ═══════════════════════════════════════════════════════════════════ */
static int test_integration(uint32_t seed) {
    int ops, i, next_block, enqueued;
    g_rng = seed;
    init_both();

    ops = 20 + (xorshift32() % 80);
    next_block = 0;
    enqueued = 0;

    for (i = 0; i < ops; i++) {
        uint32_t op = xorshift32() % 6;

        switch (op) {
        case 0: /* Push */
            if (next_block < ORACLE_MAX_BLOCKS) {
                int32_t prio = (int32_t)(xorshift32() % ORACLE_MAX_QUEUES);
                oracle_DVDPushWaitingQueue(prio, &oracle_BlockPool[next_block]);
                port_DVDPushWaitingQueue(prio, PORT_BLOCK_ADDR(next_block));
                block_queue[next_block] = (int)prio;
                next_block++;
                enqueued++;
            }
            break;

        case 1: /* Pop */
        {
            oracle_DVDCommandBlock *ob = oracle_DVDPopWaitingQueue();
            uint32_t pb = port_DVDPopWaitingQueue();
            int oid = oracle_block_id(ob);
            int pid = port_block_id(pb);
            CHECK(oid == pid, "Integration Pop: oracle=%d port=%d", oid, pid);
            if (oid >= 0) {
                block_queue[oid] = IN_NONE;
                enqueued--;
            }
            break;
        }

        case 2: /* CheckWaitingQueue */
        {
            int oc = oracle_DVDCheckWaitingQueue();
            int pc = port_DVDCheckWaitingQueue();
            CHECK(oc == pc, "Integration Check: oracle=%d port=%d", oc, pc);
            break;
        }

        case 3: /* Dequeue — only blocks currently in queue */
            if (next_block > 0 && enqueued > 0) {
                int idx = (int)(xorshift32() % (uint32_t)next_block);
                if (block_queue[idx] != IN_NONE) {
                    int or_result = oracle_DVDDequeueWaitingQueue(&oracle_BlockPool[idx]);
                    int pr_result = port_DVDDequeueWaitingQueue(PORT_BLOCK_ADDR(idx));
                    CHECK(or_result == pr_result,
                          "Integration Dequeue(%d): oracle=%d port=%d", idx, or_result, pr_result);
                    if (or_result) {
                        block_queue[idx] = IN_NONE;
                        enqueued--;
                    }
                }
            }
            break;

        case 4: /* IsBlockInWaitingQueue */
            if (next_block > 0) {
                int idx = (int)(xorshift32() % (uint32_t)next_block);
                int ov = oracle_DVDIsBlockInWaitingQueue(&oracle_BlockPool[idx]);
                int pv = port_DVDIsBlockInWaitingQueue(PORT_BLOCK_ADDR(idx));
                CHECK(ov == pv, "Integration IsIn(%d): oracle=%d port=%d", idx, ov, pv);
            }
            break;

        case 5: /* Clear + reinit */
            oracle_DVDClearWaitingQueue();
            port_DVDClearWaitingQueue();
            {
                int j;
                for (j = 0; j < next_block; j++) {
                    oracle_BlockPool[j].next = NULL;
                    oracle_BlockPool[j].prev = NULL;
                    qw32(PORT_BLOCK_ADDR(j), PORT_BLOCK_NEXT, 0);
                    qw32(PORT_BLOCK_ADDR(j), PORT_BLOCK_PREV, 0);
                    block_queue[j] = IN_NONE;
                }
            }
            next_block = 0;
            enqueued = 0;
            break;
        }
    }

    /* Final drain — compare */
    for (;;) {
        oracle_DVDCommandBlock *ob = oracle_DVDPopWaitingQueue();
        uint32_t pb = port_DVDPopWaitingQueue();
        int oid = oracle_block_id(ob);
        int pid = port_block_id(pb);
        CHECK(oid == pid, "Integration drain: oracle=%d port=%d", oid, pid);
        if (oid == -1) break;
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

    if (!test_push_pop(sub ^ 0xDEAD0001u)) return 0;
    if (!test_dequeue(sub ^ 0xDEAD0002u)) return 0;
    if (!test_is_in_queue(sub ^ 0xDEAD0003u)) return 0;
    if (!test_properties(sub ^ 0xDEAD0004u)) return 0;
    if (!test_integration(sub ^ 0xDEAD0005u)) return 0;

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
        else if (strcmp(argv[i], "-v") == 0)
            g_verbose = 1;
    }

    printf("\n=== dvdqueue Property Test ===\n");

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
