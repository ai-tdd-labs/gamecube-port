#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef int BOOL;

// sdk_port entrypoint we are validating.
BOOL PADInit(void);

static void must_read_exact(const char *path, void *dst, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) die("failed to open input bin");
    size_t got = fread(dst, 1, n, f);
    fclose(f);
    if (got != n) die("short read input bin");
}

const char *gc_scenario_label(void) { return "PADInit/rvz_trace_replay_001"; }

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
        snprintf(path, sizeof(path), "../actual/pad_init_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/pad_init_rvz_trace_replay_001.bin");
    }
    return path;
}

void gc_scenario_run(GcRam *ram) {
    const char *in_sdk = getenv("GC_TRACE_IN_SDK_STATE");
    if (!in_sdk || !*in_sdk) die("GC_TRACE_IN_SDK_STATE is required");

    // Seed sdk_state exactly as observed at PADInit entry.
    uint8_t sdk_in[0x2000];
    must_read_exact(in_sdk, sdk_in, sizeof(sdk_in));
    uint8_t *sdk_state = gc_ram_ptr(ram, 0x817FE000u, sizeof(sdk_in));
    if (!sdk_state) die("gc_ram_ptr(sdk_state) failed");
    memcpy(sdk_state, sdk_in, sizeof(sdk_in));

    BOOL ret = PADInit();

    // Output layout (0x2008 bytes):
    // 0x00 marker
    // 0x04 ret (0/1)
    // 0x08 sdk_state bytes (0x2000)
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x2008);
    if (!out) die("gc_ram_ptr(out) failed");
    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, (uint32_t)(ret ? 1u : 0u));
    memcpy(out + 0x08, sdk_state, 0x2000);
}

