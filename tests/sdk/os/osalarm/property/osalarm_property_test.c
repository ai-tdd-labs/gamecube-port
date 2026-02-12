/*
 * osalarm_property_test.c — Property-based parity test for OSAlarm.c
 *
 * Oracle: exact copy of decomp InsertAlarm/CancelAlarm/fire logic (native ptrs)
 * Port:   linked from src/sdk_port/os/OSAlarm.c (gc_mem big-endian access)
 *
 * Levels:
 *   L0 — InsertAlarm parity: insert alarms with random fire times, compare list
 *   L1 — CancelAlarm parity: insert then cancel random alarms, compare
 *   L2 — Fire parity: pop head (simulating decrementer), compare list state
 *   L3 — Properties: list always sorted, cancel idempotent, periodic re-insert
 *   L4 — Full integration: random mix of insert/cancel/fire
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* sdk_port headers */
#include "OSAlarm.h"
#include "gc_mem.h"

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
 * ORACLE — exact copy of decomp InsertAlarm/CancelAlarm (native ptrs)
 * ═══════════════════════════════════════════════════════════════════ */

#define ORACLE_MAX_ALARMS 32

typedef struct oracle_OSAlarm oracle_OSAlarm;
typedef void (*oracle_AlarmHandler)(void);

struct oracle_OSAlarm {
    oracle_AlarmHandler handler;
    int64_t fire;       /* absolute fire time */
    oracle_OSAlarm *prev;
    oracle_OSAlarm *next;
    int64_t period;     /* 0 = one-shot, >0 = periodic */
    int64_t start;      /* base time for periodic */
    int id;             /* index in pool */
};

static struct {
    oracle_OSAlarm *head;
    oracle_OSAlarm *tail;
} oracle_AlarmQueue;

static oracle_OSAlarm oracle_AlarmPool[ORACLE_MAX_ALARMS];
static int64_t oracle_SystemTime; /* simulated system time */

static void oracle_dummy_handler(void) {}

static void oracle_InsertAlarm(oracle_OSAlarm *alarm, int64_t fire, oracle_AlarmHandler handler) {
    oracle_OSAlarm *next;
    oracle_OSAlarm *prev;

    if (0 < alarm->period) {
        int64_t time = oracle_SystemTime;
        fire = alarm->start;
        if (alarm->start < time) {
            fire += alarm->period * ((time - alarm->start) / alarm->period + 1);
        }
    }

    alarm->handler = handler;
    alarm->fire = fire;

    for (next = oracle_AlarmQueue.head; next; next = next->next) {
        if (next->fire <= fire) {
            continue;
        }
        alarm->prev = next->prev;
        next->prev = alarm;
        alarm->next = next;
        prev = alarm->prev;
        if (prev) {
            prev->next = alarm;
        } else {
            oracle_AlarmQueue.head = alarm;
        }
        return;
    }

    alarm->next = 0;
    prev = oracle_AlarmQueue.tail;
    oracle_AlarmQueue.tail = alarm;
    alarm->prev = prev;
    if (prev) {
        prev->next = alarm;
    } else {
        oracle_AlarmQueue.head = oracle_AlarmQueue.tail = alarm;
    }
}

static void oracle_OSSetAlarm(oracle_OSAlarm *alarm, int64_t tick) {
    alarm->period = 0;
    oracle_InsertAlarm(alarm, oracle_SystemTime + tick, oracle_dummy_handler);
}

static void oracle_OSSetPeriodicAlarm(oracle_OSAlarm *alarm, int64_t start, int64_t period) {
    alarm->period = period;
    alarm->start = start;
    oracle_InsertAlarm(alarm, 0, oracle_dummy_handler);
}

static void oracle_OSCancelAlarm(oracle_OSAlarm *alarm) {
    oracle_OSAlarm *next;

    if (alarm->handler == 0) return;

    next = alarm->next;
    if (next == 0) {
        oracle_AlarmQueue.tail = alarm->prev;
    } else {
        next->prev = alarm->prev;
    }
    if (alarm->prev) {
        alarm->prev->next = next;
    } else {
        oracle_AlarmQueue.head = next;
    }
    alarm->handler = 0;
}

/* Simulate decrementer exception: pop head, re-insert if periodic */
static int oracle_FireHead(void) {
    oracle_OSAlarm *alarm = oracle_AlarmQueue.head;
    oracle_OSAlarm *next;
    oracle_AlarmHandler handler;

    if (!alarm) return -1;
    if (oracle_SystemTime < alarm->fire) return -1;

    next = alarm->next;
    oracle_AlarmQueue.head = next;
    if (next == 0) {
        oracle_AlarmQueue.tail = 0;
    } else {
        next->prev = 0;
    }

    handler = alarm->handler;
    alarm->handler = 0;
    if (0 < alarm->period) {
        oracle_InsertAlarm(alarm, 0, handler);
    }

    return alarm->id;
}

static void oracle_OSCreateAlarm(oracle_OSAlarm *alarm) {
    alarm->handler = 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * PORT — linked from sdk_port/os/OSAlarm.c (gc_mem big-endian)
 * ═══════════════════════════════════════════════════════════════════ */

#define PORT_MAX_ALARMS ORACLE_MAX_ALARMS

/* gc_mem backing buffer for alarm structs */
#define GC_ALARM_BASE  0x80001000u
static uint8_t gc_alarm_backing[PORT_MAX_ALARMS * PORT_ALARM_SIZE];

#define ALARM_ADDR(i) (GC_ALARM_BASE + (uint32_t)(i) * PORT_ALARM_SIZE)

/* Host-side alarm queue state */
static port_OSAlarmState port_state;

/* BE helpers for test reads (compare/init) */
static uint32_t test_load_u32be(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static void test_store_u32be(uint32_t addr, uint32_t val) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8);
    p[3] = (uint8_t)val;
}

static int64_t test_load_s64be(uint32_t addr) {
    int32_t hi = (int32_t)test_load_u32be(addr);
    uint32_t lo = test_load_u32be(addr + 4);
    return ((int64_t)hi << 32) | (uint64_t)lo;
}

static void test_store_s64be(uint32_t addr, int64_t val) {
    test_store_u32be(addr, (uint32_t)(val >> 32));
    test_store_u32be(addr + 4, (uint32_t)(val & 0xFFFFFFFFu));
}

/* Wrapper: fire head and return alarm id (or -1) */
static int port_FireHead(void) {
    uint32_t fired = port_OSAlarmFireHead(&port_state);
    if (fired == 0) return -1;
    return (int)test_load_u32be(fired + PORT_ALARM_TAG);
}

/* ═══════════════════════════════════════════════════════════════════
 * Test infrastructure
 * ═══════════════════════════════════════════════════════════════════ */

#define ACTIVE   1
#define INACTIVE 0
static int alarm_active[ORACLE_MAX_ALARMS];

static void init_both(void) {
    int i;
    oracle_AlarmQueue.head = NULL;
    oracle_AlarmQueue.tail = NULL;
    oracle_SystemTime = 1000;

    memset(gc_alarm_backing, 0, sizeof(gc_alarm_backing));
    port_OSAlarmInit(&port_state);
    port_state.systemTime = 1000;

    for (i = 0; i < ORACLE_MAX_ALARMS; i++) {
        oracle_OSCreateAlarm(&oracle_AlarmPool[i]);
        oracle_AlarmPool[i].id = i;
        oracle_AlarmPool[i].prev = NULL;
        oracle_AlarmPool[i].next = NULL;
        oracle_AlarmPool[i].period = 0;
        oracle_AlarmPool[i].start = 0;

        port_OSCreateAlarm(ALARM_ADDR(i));
        test_store_u32be(ALARM_ADDR(i) + PORT_ALARM_TAG, (uint32_t)i);
        test_store_u32be(ALARM_ADDR(i) + PORT_ALARM_PREV, 0);
        test_store_u32be(ALARM_ADDR(i) + PORT_ALARM_NEXT, 0);
        test_store_s64be(ALARM_ADDR(i) + PORT_ALARM_PERIOD, 0);
        test_store_s64be(ALARM_ADDR(i) + PORT_ALARM_START, 0);

        alarm_active[i] = INACTIVE;
    }
}

/* Compare entire alarm queue state between oracle and port */
static int compare_queues(const char *label) {
    oracle_OSAlarm *oa;
    uint32_t pa;
    int count = 0;

    oa = oracle_AlarmQueue.head;
    pa = port_state.queueHead;

    while (oa != NULL || pa != 0) {
        int oid = oa ? oa->id : -1;
        int pid = (pa != 0) ? (int)test_load_u32be(pa + PORT_ALARM_TAG) : -1;
        CHECK(oid == pid, "%s: queue mismatch at pos %d: oracle=%d port=%d", label, count, oid, pid);

        if (oa) {
            int64_t of = oa->fire;
            int64_t pf = test_load_s64be(pa + PORT_ALARM_FIRE);
            CHECK(of == pf, "%s: fire mismatch alarm %d: oracle=%lld port=%lld",
                  label, oid, (long long)of, (long long)pf);
        }

        oa = oa ? oa->next : NULL;
        pa = (pa != 0) ? test_load_u32be(pa + PORT_ALARM_NEXT) : 0;
        count++;
    }

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L0 — InsertAlarm parity
 * ═══════════════════════════════════════════════════════════════════ */
static int test_insert(uint32_t seed) {
    int n, i;
    g_rng = seed;
    init_both();

    n = 3 + (xorshift32() % 20);
    if (n > ORACLE_MAX_ALARMS) n = ORACLE_MAX_ALARMS;

    for (i = 0; i < n; i++) {
        int64_t tick = (int64_t)(xorshift32() % 10000) + 1;
        oracle_OSSetAlarm(&oracle_AlarmPool[i], tick);
        port_OSSetAlarm(&port_state, ALARM_ADDR(i), tick);
        alarm_active[i] = ACTIVE;
    }

    if (!compare_queues("L0-Insert")) return 0;

    /* Verify sorted order property */
    {
        oracle_OSAlarm *a = oracle_AlarmQueue.head;
        int64_t last = -1;
        while (a) {
            CHECK(a->fire >= last, "L0-Sorted: fire %lld < prev %lld",
                  (long long)a->fire, (long long)last);
            last = a->fire;
            a = a->next;
        }
    }

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L1 — CancelAlarm parity
 * ═══════════════════════════════════════════════════════════════════ */
static int test_cancel(uint32_t seed) {
    int n, num_cancel, i;
    g_rng = seed;
    init_both();

    n = 5 + (xorshift32() % 20);
    if (n > ORACLE_MAX_ALARMS) n = ORACLE_MAX_ALARMS;

    /* Insert alarms */
    for (i = 0; i < n; i++) {
        int64_t tick = (int64_t)(xorshift32() % 10000) + 1;
        oracle_OSSetAlarm(&oracle_AlarmPool[i], tick);
        port_OSSetAlarm(&port_state, ALARM_ADDR(i), tick);
        alarm_active[i] = ACTIVE;
    }

    /* Cancel some */
    num_cancel = 1 + (xorshift32() % n);
    for (i = 0; i < num_cancel; i++) {
        int idx = (int)(xorshift32() % (uint32_t)n);
        oracle_OSCancelAlarm(&oracle_AlarmPool[idx]);
        port_OSCancelAlarm(&port_state, ALARM_ADDR(idx));
        alarm_active[idx] = INACTIVE;
    }

    if (!compare_queues("L1-Cancel")) return 0;

    /* Cancel already-cancelled alarm (idempotent) */
    {
        int idx = (int)(xorshift32() % (uint32_t)n);
        if (!alarm_active[idx]) {
            oracle_OSCancelAlarm(&oracle_AlarmPool[idx]);
            port_OSCancelAlarm(&port_state, ALARM_ADDR(idx));
            if (!compare_queues("L1-CancelIdem")) return 0;
        }
    }

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2 — Fire parity (pop head alarm)
 * ═══════════════════════════════════════════════════════════════════ */
static int test_fire(uint32_t seed) {
    int n, fires, i;
    g_rng = seed;
    init_both();

    n = 5 + (xorshift32() % 15);
    if (n > ORACLE_MAX_ALARMS) n = ORACLE_MAX_ALARMS;

    /* Insert alarms with spread-out fire times */
    for (i = 0; i < n; i++) {
        int64_t tick = (int64_t)(xorshift32() % 5000) + 100;
        oracle_OSSetAlarm(&oracle_AlarmPool[i], tick);
        port_OSSetAlarm(&port_state, ALARM_ADDR(i), tick);
        alarm_active[i] = ACTIVE;
    }

    /* Advance time and fire alarms */
    fires = 1 + (xorshift32() % (n > 1 ? (uint32_t)n : 1));
    for (i = 0; i < fires; i++) {
        /* Advance time to first alarm's fire time */
        if (oracle_AlarmQueue.head) {
            oracle_SystemTime = oracle_AlarmQueue.head->fire;
            port_state.systemTime = oracle_SystemTime;
        }

        {
            int oid = oracle_FireHead();
            int pid = port_FireHead();
            CHECK(oid == pid, "L2-Fire step %d: oracle=%d port=%d", i, oid, pid);
        }
    }

    if (!compare_queues("L2-Fire")) return 0;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3 — Periodic alarm property
 * ═══════════════════════════════════════════════════════════════════ */
static int test_periodic(uint32_t seed) {
    int i;
    g_rng = seed;
    init_both();

    /* Set up a periodic alarm */
    {
        int64_t start = oracle_SystemTime + 100;
        int64_t period = (int64_t)(xorshift32() % 500) + 50;
        oracle_OSSetPeriodicAlarm(&oracle_AlarmPool[0], start, period);
        port_OSSetPeriodicAlarm(&port_state, ALARM_ADDR(0), start, period);
        alarm_active[0] = ACTIVE;
    }

    /* Also add some one-shot alarms */
    for (i = 1; i < 6; i++) {
        int64_t tick = (int64_t)(xorshift32() % 3000) + 50;
        oracle_OSSetAlarm(&oracle_AlarmPool[i], tick);
        port_OSSetAlarm(&port_state, ALARM_ADDR(i), tick);
        alarm_active[i] = ACTIVE;
    }

    if (!compare_queues("L3-PeriodicSetup")) return 0;

    /* Fire alarms and verify periodic re-insertion */
    for (i = 0; i < 10; i++) {
        if (oracle_AlarmQueue.head) {
            oracle_SystemTime = oracle_AlarmQueue.head->fire;
            port_state.systemTime = oracle_SystemTime;
        } else {
            break;
        }

        {
            int oid = oracle_FireHead();
            int pid = port_FireHead();
            CHECK(oid == pid, "L3-PeriodicFire step %d: oracle=%d port=%d", i, oid, pid);
        }

        if (!compare_queues("L3-PeriodicStep")) return 0;
    }

    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4 — Full integration: random mix
 * ═══════════════════════════════════════════════════════════════════ */
static int test_integration(uint32_t seed) {
    int ops, i, next_alarm;
    g_rng = seed;
    init_both();

    ops = 20 + (xorshift32() % 80);
    next_alarm = 0;

    for (i = 0; i < ops; i++) {
        uint32_t op = xorshift32() % 5;

        switch (op) {
        case 0: /* Insert one-shot */
            if (next_alarm < ORACLE_MAX_ALARMS) {
                int64_t tick = (int64_t)(xorshift32() % 10000) + 1;
                oracle_OSSetAlarm(&oracle_AlarmPool[next_alarm], tick);
                port_OSSetAlarm(&port_state, ALARM_ADDR(next_alarm), tick);
                alarm_active[next_alarm] = ACTIVE;
                next_alarm++;
            }
            break;

        case 1: /* Insert periodic */
            if (next_alarm < ORACLE_MAX_ALARMS) {
                int64_t start = oracle_SystemTime + (int64_t)(xorshift32() % 1000);
                int64_t period = (int64_t)(xorshift32() % 500) + 10;
                oracle_OSSetPeriodicAlarm(&oracle_AlarmPool[next_alarm], start, period);
                port_OSSetPeriodicAlarm(&port_state, ALARM_ADDR(next_alarm), start, period);
                alarm_active[next_alarm] = ACTIVE;
                next_alarm++;
            }
            break;

        case 2: /* Cancel active alarm */
            if (next_alarm > 0) {
                int idx = (int)(xorshift32() % (uint32_t)next_alarm);
                if (alarm_active[idx]) {
                    oracle_OSCancelAlarm(&oracle_AlarmPool[idx]);
                    port_OSCancelAlarm(&port_state, ALARM_ADDR(idx));
                    alarm_active[idx] = INACTIVE;
                }
            }
            break;

        case 3: /* Fire head */
            if (oracle_AlarmQueue.head) {
                oracle_SystemTime = oracle_AlarmQueue.head->fire;
                port_state.systemTime = oracle_SystemTime;

                {
                    int oid = oracle_FireHead();
                    int pid = port_FireHead();
                    CHECK(oid == pid, "L4-Fire: oracle=%d port=%d", oid, pid);
                }
            }
            break;

        case 4: /* Advance time */
            {
                int64_t advance = (int64_t)(xorshift32() % 2000);
                oracle_SystemTime += advance;
                port_state.systemTime += advance;
            }
            break;
        }
    }

    if (!compare_queues("L4-Integration")) return 0;

    /* Drain all by firing */
    while (oracle_AlarmQueue.head) {
        oracle_SystemTime = oracle_AlarmQueue.head->fire;
        port_state.systemTime = oracle_SystemTime;

        /* Cancel periodic alarms to avoid infinite loop */
        if (oracle_AlarmQueue.head->period > 0) {
            oracle_OSCancelAlarm(oracle_AlarmQueue.head);
            port_OSCancelAlarm(&port_state, port_state.queueHead);
        } else {
            int oid = oracle_FireHead();
            int pid = port_FireHead();
            CHECK(oid == pid, "L4-Drain: oracle=%d port=%d", oid, pid);
        }
    }

    CHECK(port_state.queueHead == 0, "L4: port queue not empty after drain");
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════
 * Runner
 * ═══════════════════════════════════════════════════════════════════ */

static int run_seed(uint32_t seed) {
    uint32_t sub;

    g_rng = seed;
    sub = xorshift32();

    if (!g_opt_op || strstr("L0", g_opt_op) || strstr("INSERT", g_opt_op)) {
        if (!test_insert(sub ^ 0xA1A20001u)) return 0;
    }
    if (!g_opt_op || strstr("L1", g_opt_op) || strstr("CANCEL", g_opt_op)) {
        if (!test_cancel(sub ^ 0xA1A20002u)) return 0;
    }
    if (!g_opt_op || strstr("L2", g_opt_op) || strstr("FIRE", g_opt_op)) {
        if (!test_fire(sub ^ 0xA1A20003u)) return 0;
    }
    if (!g_opt_op || strstr("L3", g_opt_op) || strstr("PERIODIC", g_opt_op)) {
        if (!test_periodic(sub ^ 0xA1A20004u)) return 0;
    }
    if (!g_opt_op || strstr("L4", g_opt_op) || strstr("FULL", g_opt_op) || strstr("MIX", g_opt_op)) {
        if (!test_integration(sub ^ 0xA1A20005u)) return 0;
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
                    "Usage: osalarm_property_test [--seed=N] [--num-runs=N] "
                    "[--op=L0|L1|L2|L3|L4|INSERT|CANCEL|FIRE|PERIODIC|FULL|MIX] [-v]\n");
            return 2;
        }
    }

    /* Set up gc_mem backing for alarm structs */
    gc_mem_set(GC_ALARM_BASE, sizeof(gc_alarm_backing), gc_alarm_backing);

    printf("\n=== OSAlarm Property Test ===\n");

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
