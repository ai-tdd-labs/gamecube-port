#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

uint32_t VIGetDTVStatus(void);

extern uint32_t gc_vi_disable_calls;
extern uint32_t gc_vi_restore_calls;
extern uint16_t gc_vi_regs[64];

const char *gc_scenario_label(void) { return "VIGetDTVStatus/legacy_001"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_get_dtv_status_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    const int VI_DTV_STAT = 55;
    gc_vi_disable_calls = 0;
    gc_vi_restore_calls = 0;
    gc_vi_regs[VI_DTV_STAT] = 3;

    uint32_t stat = VIGetDTVStatus();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 16);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, stat);
    wr32be(p + 0x08, gc_vi_disable_calls);
    wr32be(p + 0x0C, gc_vi_restore_calls);
}

