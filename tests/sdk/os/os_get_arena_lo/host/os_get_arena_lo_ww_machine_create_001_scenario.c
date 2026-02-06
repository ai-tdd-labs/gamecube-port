#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void OSSetArenaLo(void *addr);
void *OSGetArenaLo(void);
void OSSetArenaHi(void *addr);
void *OSGetArenaHi(void);

const char *gc_scenario_label(void) {
    return "OSGetArenaLo/ww_machine_create_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/os_get_arena_lo_ww_machine_create_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    // Mirror the DOL testcase: exercise the WW "shrink ArenaHi" branch.
    OSSetArenaLo((void*)0x80000000u);
    OSSetArenaHi((void*)0x83000000u);

    uint32_t arenaHi = (uint32_t)(uintptr_t)OSGetArenaHi();
    uint32_t arenaLo = (uint32_t)(uintptr_t)OSGetArenaLo();
    if (arenaHi > 0x81800000u && arenaHi - 0x1800000u > arenaLo) {
        OSSetArenaHi((void*)(uintptr_t)(arenaHi - 0x1800000u));
    }

    uint32_t arenaSize =
        ((uint32_t)(uintptr_t)OSGetArenaHi() - (uint32_t)(uintptr_t)OSGetArenaLo()) - 0xF0u;

    uint8_t *p0 = gc_ram_ptr(ram, 0x80300000u, 0x10);
    if (!p0) die("gc_ram_ptr failed");

    wr32be(p0 + 0x00, 0xDEADBEEFu);
    wr32be(p0 + 0x04, (uint32_t)(uintptr_t)OSGetArenaLo());
    wr32be(p0 + 0x08, (uint32_t)(uintptr_t)OSGetArenaHi());
    wr32be(p0 + 0x0C, arenaSize);
}

