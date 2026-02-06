#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

int PADInit(void);

extern uint32_t gc_pad_initialized;
extern uint32_t gc_pad_si_refresh_calls;
extern uint32_t gc_pad_register_reset_calls;
extern uint32_t gc_pad_reset_mask;
extern uint32_t gc_pad_cmd_probe_device[4];
extern uint32_t gc_pad_spec;
extern uint32_t gc_pad_fix_bits;
extern uint16_t gc_os_wireless_pad_fix_mode;

const char *gc_scenario_label(void) { return "PADInit/legacy_001"; }
const char *gc_scenario_out_path(void) { return "../actual/pad_init_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    gc_pad_initialized = 0;
    gc_pad_spec = 0;
    gc_pad_fix_bits = 0;
    gc_os_wireless_pad_fix_mode = 0;
    gc_pad_si_refresh_calls = 0;
    gc_pad_register_reset_calls = 0;
    gc_pad_reset_mask = 0;
    for (int i = 0; i < 4; ++i) gc_pad_cmd_probe_device[i] = 0;

    PADInit();

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x24);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, gc_pad_initialized);
    wr32be(p + 0x08, gc_pad_si_refresh_calls);
    wr32be(p + 0x0C, gc_pad_register_reset_calls);
    wr32be(p + 0x10, gc_pad_reset_mask);
    wr32be(p + 0x14, gc_pad_cmd_probe_device[0]);
    wr32be(p + 0x18, gc_pad_cmd_probe_device[1]);
    wr32be(p + 0x1C, gc_pad_cmd_probe_device[2]);
    wr32be(p + 0x20, gc_pad_cmd_probe_device[3]);
}

