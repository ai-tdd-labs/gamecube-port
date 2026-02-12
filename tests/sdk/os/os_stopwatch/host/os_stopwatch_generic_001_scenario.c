#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint64_t u64;

typedef struct OSStopwatch {
    char *name;
    u64 total;
    u32 hits;
    u32 running;
    u64 last;
    u64 min;
    u64 max;
} OSStopwatch;

void OSInitStopwatch(OSStopwatch *sw, char *name);
void OSStartStopwatch(OSStopwatch *sw);
void OSStopStopwatch(OSStopwatch *sw);
long long OSCheckStopwatch(OSStopwatch *sw);
void OSResetStopwatch(OSStopwatch *sw);

/* OSReport provided by OSError.c in the 'os' subsystem. */
int OSReport(const char *msg, ...);

static inline void wr64be_local(uint8_t *p, u64 v) {
    wr32be(p,     (u32)(v >> 32));
    wr32be(p + 4, (u32)(v & 0xFFFFFFFFu));
}

static inline void dump_sw(uint8_t *p, const OSStopwatch *sw) {
    wr64be_local(p + 0,  sw->total);
    wr32be(p + 8,  sw->hits);
    wr32be(p + 12, sw->running);
    wr64be_local(p + 16, sw->last);
    wr64be_local(p + 24, sw->min);
    wr64be_local(p + 32, sw->max);
}

const char *gc_scenario_label(void) { return "OS_STOPWATCH/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/os_stopwatch_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x120);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x100; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    OSStopwatch sw;
    long long check;

    /* --- T1: Init --- */
    OSInitStopwatch(&sw, "test");
    wr32be(p + off, 0x5710A001u); off += 4;
    dump_sw(p + off, &sw); off += 40;

    /* --- T2: Start --- */
    OSStartStopwatch(&sw);
    wr32be(p + off, 0x5710A002u); off += 4;
    dump_sw(p + off, &sw); off += 40;

    /* --- T3: Stop --- */
    OSStopStopwatch(&sw);
    wr32be(p + off, 0x5710A003u); off += 4;
    dump_sw(p + off, &sw); off += 40;

    /* --- T4: Start again --- */
    OSStartStopwatch(&sw);
    wr32be(p + off, 0x5710A004u); off += 4;
    dump_sw(p + off, &sw); off += 40;

    /* --- T5: Stop again --- */
    OSStopStopwatch(&sw);
    wr32be(p + off, 0x5710A005u); off += 4;
    dump_sw(p + off, &sw); off += 40;

    /* --- T6: Check while stopped --- */
    check = OSCheckStopwatch(&sw);
    wr32be(p + off, 0x5710A006u); off += 4;
    wr64be_local(p + off, (u64)check); off += 8;

    /* --- T7: Reset --- */
    OSResetStopwatch(&sw);
    wr32be(p + off, 0x5710A007u); off += 4;
    dump_sw(p + off, &sw); off += 40;
}
