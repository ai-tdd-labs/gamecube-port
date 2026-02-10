#include "gc_host_scenario.h"
#include "gc_mem.h"
#include "sdk_state.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int OSDisableInterrupts(void);

static void die(const char *msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(2);
}

static void store_u32be(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

const char *gc_scenario_label(void) { return "OSDisableInterrupts/rvz_trace_replay_001"; }

const char *gc_scenario_out_path(void) {
    // Keep this local-only (actual/*.bin is gitignored).
    // Allow callers to tag outputs per trace case id.
    const char *case_id = getenv("GC_TRACE_CASE_ID");
    static char path[512];
    if (case_id && *case_id) {
        // sanitize: only keep a conservative charset
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
        snprintf(path, sizeof(path), "../actual/os_disable_interrupts_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/os_disable_interrupts_rvz_trace_replay_001.bin");
    }
    return path;
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    // RVZ trace provides CPU MSR state (EE bit), not our synthetic sdk_state page.
    // We replay only the derived invariant:
    // - return value equals "interrupts enabled" at entry
    // - interrupts are disabled after the call
    // - OSDisableInterrupts call counter increments by 1
    const char *env_seed = getenv("GC_SEED_ENABLED");
    const char *env_expect = getenv("GC_EXPECTED_RET");
    if (!env_seed || !*env_seed) die("GC_SEED_ENABLED is required (0/1)");
    if (!env_expect || !*env_expect) die("GC_EXPECTED_RET is required (0/1)");

    int seed_enabled = (int)strtoul(env_seed, 0, 0);
    int expected_ret = (int)strtoul(env_expect, 0, 0);

    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_INTS_ENABLED, (uint32_t)(seed_enabled ? 1u : 0u));
    gc_sdk_state_store_u32be(GC_SDK_OFF_OS_DISABLE_CALLS, 0);

    uint32_t enabled_before = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED);
    int actual_ret = OSDisableInterrupts();
    uint32_t enabled_after = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_INTS_ENABLED);
    uint32_t disable_calls = gc_sdk_state_load_u32be(GC_SDK_OFF_OS_DISABLE_CALLS);

    int ok = (actual_ret == expected_ret) && (enabled_after == 0) && (disable_calls == 1);

    store_u32be(0x80300000u, ok ? 0xDEADBEEFu : 0x0BADF00Du);
    store_u32be(0x80300004u, (uint32_t)expected_ret);
    store_u32be(0x80300008u, (uint32_t)actual_ret);
    store_u32be(0x8030000Cu, enabled_before);
    store_u32be(0x80300010u, enabled_after);
    store_u32be(0x80300014u, disable_calls);
}

