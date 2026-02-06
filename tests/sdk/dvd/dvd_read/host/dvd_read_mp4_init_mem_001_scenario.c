#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef struct { uint32_t _dummy; } DVDFileInfo;

extern uint32_t gc_dvd_read_calls;
extern uint32_t gc_dvd_last_read_len;
extern uint32_t gc_dvd_last_read_off;
int DVDRead(DVDFileInfo *fi, void *addr, int len, int off);

const char *gc_scenario_label(void) { return "DVDRead/mp4_init_mem"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_read_mp4_init_mem_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_dvd_read_calls = 0;
    gc_dvd_last_read_len = 0;
    gc_dvd_last_read_off = 0;

    DVDFileInfo fi;
    int r = DVDRead(&fi, (void*)0x81230000u, 0x200, 0x10);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x14);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, (uint32_t)r);
    wr32be(p + 0x08, gc_dvd_read_calls);
    wr32be(p + 0x0C, gc_dvd_last_read_len);
    wr32be(p + 0x10, gc_dvd_last_read_off);
}
