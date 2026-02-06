#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

int OSReport(const char *msg, ...);

const char *gc_scenario_label(void) { return "OSReport/mp4_printf_001"; }
const char *gc_scenario_out_path(void) { return "../actual/os_report_mp4_printf_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    int n = OSReport("hello %d", 123);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, (uint32_t)n);
}
