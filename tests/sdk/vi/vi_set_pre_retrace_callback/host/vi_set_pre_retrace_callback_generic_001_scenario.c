#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);

typedef void (*VIRetraceCallback)(uint32_t retraceCount);
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb);

#define CB_A ((VIRetraceCallback)(uintptr_t)0x80001234u)
#define CB_B ((VIRetraceCallback)(uintptr_t)0x80005678u)

const char *gc_scenario_label(void) { return "VISetPreRetraceCallback/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/vi_set_pre_retrace_callback_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    int lvl = OSDisableInterrupts();
    void *prev0 = (void *)VISetPreRetraceCallback(CB_A);
    OSRestoreInterrupts(lvl);

    lvl = OSDisableInterrupts();
    void *prev1 = (void *)VISetPreRetraceCallback(CB_B);
    OSRestoreInterrupts(lvl);

    uint32_t cb_ptr = gc_sdk_state_load_u32be(GC_SDK_OFF_VI_PRE_CB_PTR);
    uint32_t set_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_VI_PRE_CB_SET_CALLS);
    uint32_t os_disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
    uint32_t os_restore_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x1C);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, (uint32_t)(uintptr_t)prev0);
    wr32be(p + 0x08, (uint32_t)(uintptr_t)prev1);
    wr32be(p + 0x0C, cb_ptr);
    wr32be(p + 0x10, set_calls);
    wr32be(p + 0x14, os_disable_calls);
    wr32be(p + 0x18, os_restore_calls);
}
