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

const char *gc_scenario_label(void) { return "OSStopwatch/mp4_huperf"; }
const char *gc_scenario_out_path(void) { return "../actual/os_stopwatch_mp4_huperf_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    OSStopwatch sw;
    OSInitStopwatch(&sw, (char*)"CPU");

    OSStopStopwatch(&sw);
    OSResetStopwatch(&sw);
    OSStartStopwatch(&sw);
    long long t0 = OSCheckStopwatch(&sw);
    OSStopStopwatch(&sw);
    long long t1 = OSCheckStopwatch(&sw);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, sw.hits);
    wr32be(p + 0x08, sw.running);
    wr32be(p + 0x0C, (u32)(sw.total & 0xFFFFFFFFu));
    wr32be(p + 0x10, (u32)(sw.min & 0xFFFFFFFFu));
    wr32be(p + 0x14, (u32)(sw.max & 0xFFFFFFFFu));
    wr32be(p + 0x18, (u32)(t0 & 0xFFFFFFFFu));
    wr32be(p + 0x1C, (u32)(t1 & 0xFFFFFFFFu));
    for (u32 off = 0x20; off < 0x40; off += 4) {
        wr32be(p + off, 0);
    }
}
