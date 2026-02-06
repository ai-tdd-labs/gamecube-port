#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_vi_flush_calls;
void VIFlush(void);

const char *gc_scenario_label(void) { return "VIFlush/mp4_init_vi"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_flush_mp4_init_vi_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_vi_flush_calls = 0;
    VIFlush();
    VIFlush();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_vi_flush_calls);
}
