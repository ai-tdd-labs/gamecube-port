#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void *OSGetArenaLo(void);

const char *gc_scenario_label(void) {
    return "OSGetArenaLo/edge_default_uninit_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/os_get_arena_lo_edge_default_uninit_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    // In our sdk_port, arenaLo defaults to (void*)-1 (matches MP4 decomp init).
    uint8_t *p0 = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p0) die("gc_ram_ptr failed");

    wr32be(p0 + 0, 0xDEADBEEFu);
    wr32be(p0 + 4, (uint32_t)(uintptr_t)OSGetArenaLo());
}

