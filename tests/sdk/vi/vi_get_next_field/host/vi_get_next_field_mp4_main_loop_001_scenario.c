#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_vi_next_field;
uint32_t VIGetNextField(void);

const char *gc_scenario_label(void) { return "VIGetNextField/mp4_main_loop"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_get_next_field_mp4_main_loop_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_vi_next_field = 0;
    uint32_t v = VIGetNextField();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, v);
}
