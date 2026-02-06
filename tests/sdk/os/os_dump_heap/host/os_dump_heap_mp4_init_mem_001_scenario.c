#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_os_dump_heap_calls;
void OSDumpHeap(void);

const char *gc_scenario_label(void) { return "OSDumpHeap/mp4_init_mem"; }
const char *gc_scenario_out_path(void) { return "../actual/os_dump_heap_mp4_init_mem_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_os_dump_heap_calls = 0;
    OSDumpHeap();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, gc_os_dump_heap_calls);
}
