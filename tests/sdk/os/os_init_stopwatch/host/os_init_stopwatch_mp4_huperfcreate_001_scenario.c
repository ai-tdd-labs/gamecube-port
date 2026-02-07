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

const char *gc_scenario_label(void) { return "os_init_stopwatch/mp4_huperfcreate"; }
const char *gc_scenario_out_path(void) { return "../actual/os_init_stopwatch_mp4_huperfcreate_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    OSStopwatch sw;

    sw.name = (char*)0;
    sw.total = 0x1111111122222222ull;
    sw.hits = 0x33333333u;
    sw.running = 0x44444444u;
    sw.last = 0x5555555566666666ull;
    sw.min = 0x7777777788888888ull;
    sw.max = 0x99999999AAAAAAAAull;

    OSInitStopwatch(&sw, (char*)"CPU");

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x30);
    if (!p) die("gc_ram_ptr failed");

    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, 0); // host pointer value is meaningless across environments
    wr32be(p + 0x08, (u32)(sw.total >> 32));
    wr32be(p + 0x0C, (u32)(sw.total & 0xFFFFFFFFu));
    wr32be(p + 0x10, sw.hits);
    wr32be(p + 0x14, (u32)(sw.min >> 32));
    wr32be(p + 0x18, (u32)(sw.min & 0xFFFFFFFFu));
    wr32be(p + 0x1C, (u32)(sw.max >> 32));
    wr32be(p + 0x20, (u32)(sw.max & 0xFFFFFFFFu));

    // Untouched fields sentinel.
    wr32be(p + 0x24, sw.running);
    wr32be(p + 0x28, (u32)(sw.last >> 32));
    wr32be(p + 0x2C, (u32)(sw.last & 0xFFFFFFFFu));
}
