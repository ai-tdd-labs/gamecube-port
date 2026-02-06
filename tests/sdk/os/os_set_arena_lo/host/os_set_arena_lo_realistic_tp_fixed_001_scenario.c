#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void OSSetArenaLo(void *addr);
void *OSGetArenaLo(void);

const char *gc_scenario_label(void) {
    return "OSSetArenaLo/realistic_tp_fixed_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/os_set_arena_lo_realistic_tp_fixed_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    // Matches the DOL testcase for TP: OSExec/OSReboot fixed value.
    OSSetArenaLo((void*)0x81280000u);

    uint8_t *p0 = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p0) die("gc_ram_ptr failed");

    wr32be(p0 + 0, 0xDEADBEEFu);
    wr32be(p0 + 4, (uint32_t)(uintptr_t)OSGetArenaLo());
}

