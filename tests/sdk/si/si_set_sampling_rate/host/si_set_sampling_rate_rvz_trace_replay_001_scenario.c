#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"
#
#include "sdk_state.h"
#
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#
void VIInit(void);
void SISetSamplingRate(uint32_t msec);
#
static void die_env(const char *key) {
    fprintf(stderr, "fatal: %s is required\n", key);
    exit(2);
}
#
static uint32_t must_u32(const char *key) {
    const char *s = getenv(key);
    if (!s || !*s) die_env(key);
    char *endp = 0;
    uint32_t v = (uint32_t)strtoul(s, &endp, 0);
    if (!endp || *endp != 0) die("invalid env var");
    return v;
}
#
static uint16_t must_u16(const char *key) {
    uint32_t v = must_u32(key);
    return (uint16_t)(v & 0xFFFFu);
}
#
const char *gc_scenario_label(void) { return "SISetSamplingRate/rvz_trace_replay_001"; }
#
const char *gc_scenario_out_path(void) {
    // Keep this local-only (actual/*.bin is gitignored).
    // Allow callers to tag outputs per trace case id.
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
        snprintf(path, sizeof(path), "../actual/si_set_sampling_rate_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/si_set_sampling_rate_rvz_trace_replay_001.bin");
    }
    return path;
}
#
void gc_scenario_run(GcRam *ram) {
    // Seed inputs from a retail RVZ trace case.
    uint32_t tv_format = must_u32("GC_SEED_TV_FORMAT");
    uint16_t vi54 = must_u16("GC_SEED_VI54");
    uint32_t msec = must_u32("GC_SEED_MSEC");
#
    (void)ram;
#
    // Keep deterministic: seed interrupt state + clear counters.
    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_INTS_ENABLED, 1u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_DISABLE_CALLS, 0u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_RESTORE_CALLS, 0u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_SI_SETXY_CALLS, 0u);
#
    VIInit();
    gc_sdk_state_store_u32be(GC_SDK_OFF_VI_TV_FORMAT, tv_format);
    gc_sdk_state_store_u16be_mirror(GC_SDK_OFF_VI_REGS_U16BE + (54u * 2u), 0, vi54);
#
    SISetSamplingRate(msec);
#
    // Snapshot the relevant state into the canonical out window.
    uint32_t rate = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SAMPLING_RATE);
    uint32_t line = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_LINE);
    uint32_t count = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_COUNT);
    uint32_t setxy_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_SI_SETXY_CALLS);
#
    uint32_t ints_enabled = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED);
    uint32_t disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);
    uint32_t restore_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_RESTORE_CALLS);
#
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");
#
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, rate);
    wr32be(p + 0x08, line);
    wr32be(p + 0x0C, count);
    wr32be(p + 0x10, setxy_calls);
    wr32be(p + 0x14, ints_enabled);
    wr32be(p + 0x18, disable_calls);
    wr32be(p + 0x1C, restore_calls);
    wr32be(p + 0x20, (uint32_t)vi54);
    wr32be(p + 0x24, tv_format);
    wr32be(p + 0x28, msec);
}

