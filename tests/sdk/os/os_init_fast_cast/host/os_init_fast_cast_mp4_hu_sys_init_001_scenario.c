#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void OSInitFastCast(void);

const char *gc_scenario_label(void) { return "OSInitFastCast/mp4_hu_sys_init"; }
const char *gc_scenario_out_path(void) { return "../actual/os_init_fast_cast_mp4_hu_sys_init_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    OSInitFastCast();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, 0u);
}
