#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void OSInit(void);
void *OSGetArenaLo(void);
void *OSGetArenaHi(void);

const char *gc_scenario_label(void) { return "OSInit/legacy_001"; }
const char *gc_scenario_out_path(void) { return "../actual/os_init_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    OSInit();
    void *lo = OSGetArenaLo();
    void *hi = OSGetArenaHi();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 12);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, (uint32_t)(uintptr_t)lo);
    wr32be(p + 8, (uint32_t)(uintptr_t)hi);
}

