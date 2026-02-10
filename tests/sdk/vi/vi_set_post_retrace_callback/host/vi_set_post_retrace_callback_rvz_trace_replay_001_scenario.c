#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

typedef void (*VIRetraceCallback)(uint32_t retraceCount);
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb);

static uint32_t must_u32(const char *key) {
    const char *s = getenv(key);
    if (!s || !*s) die("missing env var");
    char *endp = 0;
    uint32_t v = (uint32_t)strtoul(s, &endp, 0);
    if (!endp || *endp != 0) die("invalid env var");
    return v;
}

const char *gc_scenario_label(void) { return "VISetPostRetraceCallback/rvz_trace_replay_001"; }

const char *gc_scenario_out_path(void) {
    const char *case_id = getenv("GC_TRACE_CASE_ID");
    static char path[512];
    if (case_id && *case_id) {
        char safe[256];
        size_t j = 0;
        for (size_t i = 0; case_id[i] && j + 1 < sizeof(safe); i++) {
            char c = case_id[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_' || c == '-' || c == '.') {
                safe[j++] = c;
            }
        }
        safe[j] = 0;
        snprintf(path, sizeof(path), "../actual/vi_set_post_retrace_callback_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/vi_set_post_retrace_callback_rvz_trace_replay_001.bin");
    }
    return path;
}

void gc_scenario_run(GcRam *ram) {
    uint32_t seed_old = must_u32("GC_SEED_OLD_CB");
    uint32_t seed_new = must_u32("GC_SEED_NEW_CB");

    // Ensure deterministic counters.
    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_DISABLE_CALLS, 0u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_RESTORE_CALLS, 0u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_VI_POST_CB_SET_CALLS, 0u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_VI_POST_CB_PTR, 0u);

    // Seed prior callback state if the retail trace had a non-null old callback.
    if (seed_old) {
        (void)VISetPostRetraceCallback((VIRetraceCallback)(uintptr_t)seed_old);
    }

    void *prev = (void *)VISetPostRetraceCallback((VIRetraceCallback)(uintptr_t)seed_new);

    uint32_t cb_ptr = gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_PTR);
    uint32_t set_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_VI_POST_CB_SET_CALLS);
    uint32_t os_disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
    uint32_t os_restore_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, (uint32_t)(uintptr_t)prev);
    wr32be(p + 0x08, cb_ptr);
    wr32be(p + 0x0C, set_calls);
    wr32be(p + 0x10, os_disable_calls);
    wr32be(p + 0x14, os_restore_calls);
    wr32be(p + 0x18, seed_old);
    wr32be(p + 0x1C, seed_new);
}

