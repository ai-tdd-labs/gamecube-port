#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;

// sdk_port entrypoint we are validating.
void PADSetSpec(u32 spec);

static u32 must_u32_hex(const char *key) {
    const char *s = getenv(key);
    if (!s || !*s) die("missing env var");
    char *endp = 0;
    u32 v = (u32)strtoul(s, &endp, 16);
    if (!endp || *endp != 0) die("invalid env var");
    return v;
}

static void must_read_exact(const char *path, void *dst, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) die("failed to open input bin");
    size_t got = fread(dst, 1, n, f);
    fclose(f);
    if (got != n) die("short read input bin");
}

const char *gc_scenario_label(void) { return "PADSetSpec/rvz_trace_replay_001"; }

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
        snprintf(path, sizeof(path), "../actual/pad_set_spec_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/pad_set_spec_rvz_trace_replay_001.bin");
    }
    return path;
}

void gc_scenario_run(GcRam *ram) {
    u32 spec = must_u32_hex("GC_TRACE_SPEC");
    const char *in_shadow = getenv("GC_TRACE_IN_PADSPEC_SHADOW");
    const char *in_spec = getenv("GC_TRACE_IN_SPEC");
    const char *in_ms = getenv("GC_TRACE_IN_MAKE_STATUS");
    if (!in_shadow || !*in_shadow) die("GC_TRACE_IN_PADSPEC_SHADOW is required");
    if (!in_spec || !*in_spec) die("GC_TRACE_IN_SPEC is required");
    if (!in_ms || !*in_ms) die("GC_TRACE_IN_MAKE_STATUS is required");

    // Seed retail globals exactly as observed at PADSetSpec entry.
    uint8_t buf_shadow[4];
    uint8_t buf_spec[4];
    uint8_t buf_ms[4];
    must_read_exact(in_shadow, buf_shadow, sizeof(buf_shadow));
    must_read_exact(in_spec, buf_spec, sizeof(buf_spec));
    must_read_exact(in_ms, buf_ms, sizeof(buf_ms));

    uint8_t *p_shadow = gc_ram_ptr(ram, 0x801D450Cu, 4);
    uint8_t *p_spec = gc_ram_ptr(ram, 0x801D3924u, 4);
    uint8_t *p_ms = gc_ram_ptr(ram, 0x801D3928u, 4);
    if (!p_shadow) die("gc_ram_ptr(__PADSpec) failed");
    if (!p_spec) die("gc_ram_ptr(Spec) failed");
    if (!p_ms) die("gc_ram_ptr(MakeStatus) failed");
    memcpy(p_shadow, buf_shadow, 4);
    memcpy(p_spec, buf_spec, 4);
    memcpy(p_ms, buf_ms, 4);

    PADSetSpec(spec);

    // Output layout (0x14 bytes):
    // 0x00 marker
    // 0x04 spec (u32, from entry regs)
    // 0x08 __PADSpec (u32, retail global shadow; should be 0)
    // 0x0C Spec (u32, retail global)
    // 0x10 MakeStatus (u32, retail global function pointer)
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x14);
    if (!out) die("gc_ram_ptr(out) failed");
    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, spec);
    memcpy(out + 0x08, p_shadow, 4);
    memcpy(out + 0x0C, p_spec, 4);
    memcpy(out + 0x10, p_ms, 4);
}
