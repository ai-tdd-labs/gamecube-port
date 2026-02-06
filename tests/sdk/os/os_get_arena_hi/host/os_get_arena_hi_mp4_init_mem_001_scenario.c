#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void *OSGetArenaHi(void);
void OSSetArenaHi(void *addr);

const char *gc_scenario_label(void) { return "OSGetArenaHi/mp4_init_mem"; }
const char *gc_scenario_out_path(void) { return "../actual/os_get_arena_hi_mp4_init_mem_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    OSSetArenaHi((void*)0x81800000);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, (uint32_t)(uintptr_t)OSGetArenaHi());
}
