#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef struct { uint32_t _dummy; } DVDFileInfo;

extern uint32_t gc_dvd_open_calls;
int DVDOpen(const char *path, DVDFileInfo *fi);

const char *gc_scenario_label(void) { return "DVDOpen/mp4_init_mem"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_open_mp4_init_mem_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_dvd_open_calls = 0;
    DVDFileInfo fi;
    int ok = DVDOpen("/meminfo.bin", &fi);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, (uint32_t)ok);
    wr32be(p + 0x08, gc_dvd_open_calls);
}
