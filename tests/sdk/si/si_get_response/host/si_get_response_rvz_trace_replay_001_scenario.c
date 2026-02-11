#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"
#
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;
#
// sdk_port entrypoint we are validating.
BOOL SIGetResponse(s32 chan, void *data);
#
// Deterministic seeds for SIGetResponseRaw() modeling.
void gc_si_set_status_seed(u32 chan, u32 status);
void gc_si_set_resp_words_seed(u32 chan, u32 word0, u32 word1);
#
// OS interrupt primitives (used to sanity-check sdk_state stability).
int OSDisableInterrupts(void);
int OSRestoreInterrupts(int enabled);
#
static u32 must_u32(const char *key) {
    const char *s = getenv(key);
    if (!s || !*s) die("missing env var");
    char *endp = 0;
    u32 v = (u32)strtoul(s, &endp, 0);
    if (!endp || *endp != 0) die("invalid env var");
    return v;
}
#
static void must_read_exact(const char *path, void *dst, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) die("failed to open input bin");
    size_t got = fread(dst, 1, n, f);
    fclose(f);
    if (got != n) die("short read input bin");
}
#
const char *gc_scenario_label(void) { return "SIGetResponse/rvz_trace_replay_001"; }
#
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
        snprintf(path, sizeof(path), "../actual/si_get_response_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/si_get_response_rvz_trace_replay_001.bin");
    }
    return path;
}
#
void gc_scenario_run(GcRam *ram) {
    // Inputs (from retail RVZ trace).
    u32 chan = must_u32("GC_TRACE_CHAN");
    u32 data_ptr = must_u32("GC_TRACE_DATA_PTR");
    u32 status = must_u32("GC_TRACE_SI_STATUS");
    u32 w0 = must_u32("GC_TRACE_SI_WORD0");
    u32 w1 = must_u32("GC_TRACE_SI_WORD1");
#
    const char *in_data = getenv("GC_TRACE_IN_DATA");
    const char *in_valid = getenv("GC_TRACE_IN_INPUTBUF_VALID");
    const char *in_buf = getenv("GC_TRACE_IN_INPUTBUF");
    const char *in_vc = getenv("GC_TRACE_IN_INPUTBUF_VCOUNT");
    if (!in_data || !in_valid || !in_buf || !in_vc) die("missing input paths");
#
    // Seed memory windows exactly as observed in retail.
    u8 buf_data[0x08];
    u8 buf_valid[0x10];
    u8 buf_buf[0x20];
    u8 buf_vc[0x10];
    must_read_exact(in_data, buf_data, sizeof(buf_data));
    must_read_exact(in_valid, buf_valid, sizeof(buf_valid));
    must_read_exact(in_buf, buf_buf, sizeof(buf_buf));
    must_read_exact(in_vc, buf_vc, sizeof(buf_vc));
#
    u8 *p_data = gc_ram_ptr(ram, data_ptr, sizeof(buf_data));
    if (!p_data) die("gc_ram_ptr(data_ptr) failed");
    memcpy(p_data, buf_data, sizeof(buf_data));
#
    // Retail MP4 SI internal globals are at these fixed addresses.
    u8 *p_valid = gc_ram_ptr(ram, 0x801A7148u, sizeof(buf_valid));
    if (!p_valid) die("gc_ram_ptr(inputbuf_valid) failed");
    memcpy(p_valid, buf_valid, sizeof(buf_valid));
#
    u8 *p_buf = gc_ram_ptr(ram, 0x801A7158u, sizeof(buf_buf));
    if (!p_buf) die("gc_ram_ptr(inputbuf) failed");
    memcpy(p_buf, buf_buf, sizeof(buf_buf));
#
    u8 *p_vc = gc_ram_ptr(ram, 0x801A7178u, sizeof(buf_vc));
    if (!p_vc) die("gc_ram_ptr(inputbuf_vcount) failed");
    memcpy(p_vc, buf_vc, sizeof(buf_vc));
#
    // Seed MMIO-derived response/status for deterministic SIGetResponseRaw.
    gc_si_set_status_seed(chan, status);
    gc_si_set_resp_words_seed(chan, w0, w1);

    // Invariant: seeds must be visible in RAM-backed sdk_state.
    // If this fails, the replay is meaningless (SIGetResponseRaw would be unseeded).
    {
        u8 *p = gc_ram_ptr(ram, 0x817FE000u + 0x3C0u + chan * 4u, 4);
        if (!p) die("gc_ram_ptr(sdk_state.status) failed");
        u32 got_status = rd32be(p);
        if (got_status != status) die("sdk_state.status seed mismatch");
    }
    {
        u8 *p = gc_ram_ptr(ram, 0x817FE000u + 0x3D0u + chan * 8u, 8);
        if (!p) die("gc_ram_ptr(sdk_state.words) failed");
        u32 got0 = rd32be(p + 0);
        u32 got1 = rd32be(p + 4);
        if (got0 != w0 || got1 != w1) die("sdk_state.words seed mismatch");
    }
#
    // Sanity: OSDisableInterrupts must not clobber unrelated sdk_state slots.
    {
        u8 *p = gc_ram_ptr(ram, 0x817FE000u + 0x3C0u + chan * 4u, 4);
        if (!p) die("gc_ram_ptr(sdk_state.status2) failed");
        int lvl = OSDisableInterrupts();
        u32 after = rd32be(p);
        OSRestoreInterrupts(lvl);
        if (after != status) die("OSDisableInterrupts clobbered SI status seed");
    }
#
    BOOL ret = SIGetResponse((s32)chan, (void *)(uintptr_t)data_ptr);
#
    // Output layout for replay comparison:
    // 0x00 marker
    // 0x04 ret
    // 0x08 chan
    // 0x0C data_ptr
    // 0x10 data (0x08) [at data_ptr]
    // 0x18 inputbuf_valid (0x10) [0x801A7148]
    // 0x28 inputbuf (0x20) [0x801A7158]
    // 0x48 inputbuf_vcount (0x10) [0x801A7178]
    u8 *out = gc_ram_ptr(ram, 0x80300000u, 0x58);
    if (!out) die("gc_ram_ptr(out) failed");
    memset(out, 0, 0x58);
#
    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, (u32)ret);
    wr32be(out + 0x08, chan);
    wr32be(out + 0x0C, data_ptr);
#
    memcpy(out + 0x10, gc_ram_ptr(ram, data_ptr, 0x08), 0x08);
    memcpy(out + 0x18, gc_ram_ptr(ram, 0x801A7148u, 0x10), 0x10);
    memcpy(out + 0x28, gc_ram_ptr(ram, 0x801A7158u, 0x20), 0x20);
    memcpy(out + 0x48, gc_ram_ptr(ram, 0x801A7178u, 0x10), 0x10);
}
