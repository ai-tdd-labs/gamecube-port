#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void OSSetArenaLo(void *addr);
void *OSGetArenaLo(void);

static uint32_t round_up_0x20(uint32_t v) { return (v + 0x1Fu) & 0xFFFFFFE0u; }

const char *gc_scenario_label(void) {
    return "OSGetArenaLo/realistic_stack_round_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/os_get_arena_lo_realistic_stack_round_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    uint32_t stack_addr = 0x81234523u;
    OSSetArenaLo((void*)round_up_0x20(stack_addr));

    uint8_t *p0 = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p0) die("gc_ram_ptr failed");

    wr32be(p0 + 0, 0xDEADBEEFu);
    wr32be(p0 + 4, (uint32_t)(uintptr_t)OSGetArenaLo());
}

