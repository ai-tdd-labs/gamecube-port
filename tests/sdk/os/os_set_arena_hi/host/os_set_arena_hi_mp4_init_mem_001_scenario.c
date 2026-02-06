#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void OSSetArenaHi(void *addr);
void *OSGetArenaHi(void);

const char *gc_scenario_label(void) { return "OSSetArenaHi/mp4_init_mem"; }
const char *gc_scenario_out_path(void) { return "../actual/os_set_arena_hi_mp4_init_mem_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    OSSetArenaHi((void*)0x81654320u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, (uint32_t)(uintptr_t)OSGetArenaHi());
}
