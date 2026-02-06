#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

uint32_t OSGetProgressiveMode(void);

extern uint8_t gc_sram_flags;
extern uint32_t gc_sram_unlock_calls;

const char *gc_scenario_label(void) { return "OSGetProgressiveMode/legacy_001"; }
const char *gc_scenario_out_path(void) { return "../actual/os_get_progressive_mode_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_sram_flags = 0x80u;
    gc_sram_unlock_calls = 0;

    uint32_t mode = OSGetProgressiveMode();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 12);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, mode);
    wr32be(p + 8, gc_sram_unlock_calls);
}

