#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"

#include "gc_mem.h"

// MP4 process system (decomp slice compiled via tools/run_host_scenario.sh).
void HuPrcInit(void);
void HuPrcCall(int tick);
void HuPrcSleep(int32_t time);
void HuPrcVSleep(void);
void *HuPrcCreate(void (*func)(void), uint16_t prio, uint32_t stack_size, int32_t extra_size);

enum {
    OUT_BASE = 0x80300000u,
    OUT_MARKER = OUT_BASE + 0x00,
    OUT_DONE = OUT_BASE + 0x04,
    OUT_COUNT_A = OUT_BASE + 0x08,
    OUT_COUNT_B = OUT_BASE + 0x0C,
    OUT_SEQ_IDX = OUT_BASE + 0x10,
    OUT_SEQ_BASE = OUT_BASE + 0x14, // u32 entries
};

static inline void store_u32be(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline uint32_t load_u32be(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return 0;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void seq_push(uint32_t tag) {
    uint32_t idx = load_u32be(OUT_SEQ_IDX);
    if (idx < 10) store_u32be(OUT_SEQ_BASE + idx * 4u, tag);
    store_u32be(OUT_SEQ_IDX, idx + 1u);
}

static void ProcA(void) {
    seq_push(0xA0000001u);
    store_u32be(OUT_COUNT_A, load_u32be(OUT_COUNT_A) + 1u);

    // Sleep for 2 ticks, yielding back to scheduler.
    HuPrcSleep(2);

    // After wake, run once more and return.
    seq_push(0xA0000002u);
    store_u32be(OUT_COUNT_A, load_u32be(OUT_COUNT_A) + 1u);
}

static void ProcB(void) {
    seq_push(0xB0000001u);
    store_u32be(OUT_COUNT_B, load_u32be(OUT_COUNT_B) + 1u);
}

const char *gc_scenario_label(void) { return "workload/mp4_process_sleep_001"; }
const char *gc_scenario_out_path(void) {
    return "../../actual/workload/mp4_process_sleep_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    // Clear output.
    for (uint32_t off = 0; off < 0x40; off += 4) store_u32be(OUT_BASE + off, 0);
    store_u32be(OUT_MARKER, 0x50524353u); // "PRCS"

    HuPrcInit();

    // B has higher prio (runs first when both are runnable).
    (void)HuPrcCreate(ProcA, 10, 8192, 0);
    (void)HuPrcCreate(ProcB, 20, 8192, 0);

    // Drive scheduler across sleep boundary.
    // Tick 0: B then A(part1 -> sleep)
    // Tick 1: A sleeping
    // Tick 2: A wakes and runs part2
    for (int i = 0; i < 6; i++) {
        HuPrcCall(1);
    }

    store_u32be(OUT_DONE, 1u);
}

