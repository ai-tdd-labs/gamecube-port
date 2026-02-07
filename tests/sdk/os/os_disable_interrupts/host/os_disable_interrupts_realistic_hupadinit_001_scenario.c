#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);

const char *gc_scenario_label(void) { return "OSDisableInterrupts/mp4_realistic_hupadinit"; }
const char *gc_scenario_out_path(void) { return "../actual/os_disable_interrupts_realistic_hupadinit_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    int outer = OSDisableInterrupts();
    int inner = OSDisableInterrupts();
    int prev1 = OSRestoreInterrupts(inner);
    int prev2 = OSRestoreInterrupts(outer);

    uint32_t enabled = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED);
    uint32_t disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
    uint32_t restore_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, (uint32_t)outer);
    wr32be(p + 0x08, (uint32_t)inner);
    wr32be(p + 0x0C, (uint32_t)prev1);
    wr32be(p + 0x10, (uint32_t)prev2);
    wr32be(p + 0x14, enabled);
    wr32be(p + 0x18, disable_calls);
    wr32be(p + 0x1C, restore_calls);
}

