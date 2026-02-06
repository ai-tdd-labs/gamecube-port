#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

uint32_t OSRoundUp32B(uint32_t x);

const char *gc_scenario_label(void) { return "OSRoundUp32B/mp4_init_mem"; }
const char *gc_scenario_out_path(void) { return "../actual/os_round_up_32b_mp4_init_mem_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    uint32_t out = OSRoundUp32B(0x80000001u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, out);
}
