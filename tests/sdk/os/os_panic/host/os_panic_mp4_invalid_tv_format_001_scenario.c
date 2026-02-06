#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_os_panic_calls;
void OSPanic(const char *file, int line, const char *msg, ...);

const char *gc_scenario_label(void) { return "OSPanic/mp4_invalid_tv_format"; }
const char *gc_scenario_out_path(void) { return "../actual/os_panic_mp4_invalid_tv_format_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_os_panic_calls = 0;
    OSPanic("init.c", 167, "DEMOInit: invalid TV format\n");

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_os_panic_calls);
}
