#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void DVDInit(void);
uint32_t DVDGetDriveStatus(void);

const char *gc_scenario_label(void) { return "DVDGetDriveStatus/mp4_realistic_hudvderrorwatch"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_get_drive_status_realistic_hudvderrorwatch_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    DVDInit();
    uint32_t st = DVDGetDriveStatus();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 8);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, st);
}
