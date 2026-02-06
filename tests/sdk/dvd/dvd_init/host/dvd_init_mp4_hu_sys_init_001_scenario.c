#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void DVDInit(void);
uint32_t DVDGetDriveStatus(void);

const char *gc_scenario_label(void) { return "DVDInit/mp4_hu_sys_init"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_init_mp4_hu_sys_init_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    DVDInit();
    uint32_t st = DVDGetDriveStatus();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, st);
}

