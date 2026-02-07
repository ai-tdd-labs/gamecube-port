#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

void SISetSamplingRate(uint32_t msec);

const char *gc_scenario_label(void) { return "SISetSamplingRate/mp4_realistic_hupadinit"; }
const char *gc_scenario_out_path(void) { return "../actual/si_set_sampling_rate_realistic_hupadinit_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // Factor=1 for VIRegs[54] bit0=0 (matches the typical HuPadInit context).
    gc_sdk_state_store_u16be_mirror(GC_SDK_OFF_VI_REGS_U16BE + (54u * 2u), 0, 0);

    SISetSamplingRate(0);

    uint32_t rate = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SAMPLING_RATE);
    uint32_t line = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_LINE);
    uint32_t count = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_COUNT);
    uint32_t setxy_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_CALLS);

    uint32_t ints_enabled = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED);
    uint32_t disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
    uint32_t restore_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, rate);
    wr32be(p + 0x08, line);
    wr32be(p + 0x0C, count);
    wr32be(p + 0x10, setxy_calls);
    wr32be(p + 0x14, ints_enabled);
    wr32be(p + 0x18, disable_calls);
    wr32be(p + 0x1C, restore_calls);
}

