#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_os_alloc_fixed_calls;
extern uint32_t gc_os_alloc_fixed_last_size;
void *OSAllocFixed(uint32_t size);

const char *gc_scenario_label(void) { return "OSAllocFixed/mp4_init_mem"; }
const char *gc_scenario_out_path(void) { return "../actual/os_alloc_fixed_mp4_init_mem_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_os_alloc_fixed_calls = 0;
    gc_os_alloc_fixed_last_size = 0;
    (void)OSAllocFixed(0x40);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_os_alloc_fixed_calls);
    wr32be(p + 0x08, gc_os_alloc_fixed_last_size);
}
