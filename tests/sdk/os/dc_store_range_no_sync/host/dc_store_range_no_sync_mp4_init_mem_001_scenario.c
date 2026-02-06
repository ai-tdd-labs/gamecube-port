#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

extern uint32_t gc_dc_store_last_addr;
extern uint32_t gc_dc_store_last_len;

void DCStoreRangeNoSync(void *addr, uint32_t nbytes);

const char *gc_scenario_label(void) { return "DCStoreRangeNoSync/mp4_init_mem"; }
const char *gc_scenario_out_path(void) { return "../actual/dc_store_range_no_sync_mp4_init_mem_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    DCStoreRangeNoSync((void*)0x81234560u, 0x1234u);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x0C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_dc_store_last_addr);
    wr32be(p + 0x08, gc_dc_store_last_len);
}
