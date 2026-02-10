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
typedef uint64_t u64;
typedef int32_t s32;
typedef int BOOL;
#
// sdk_port entrypoint we are validating.
BOOL SITransfer(s32 chan, void *output, u32 outputBytes, void *input, u32 inputBytes, void *callback, u64 delay);
#
// Deterministic seed for __OSGetSystemTime() modeling used by SITransfer.
void gc_os_set_system_time_seed(u64 system_time);
// Delta in ticks between SITransfer's `now` read and OSSetAlarm's internal time read.
void gc_os_set_setalarm_delta(u32 delta_ticks);
// Whether the immediate __SITransfer() hardware attempt should succeed.
void gc_si_set_hw_xfer_ok(u32 ok);
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
static u64 must_u64(const char *key) {
    const char *s = getenv(key);
    if (!s || !*s) die("missing env var");
    char *endp = 0;
    u64 v = (u64)strtoull(s, &endp, 0);
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
const char *gc_scenario_label(void) { return "SITransfer/rvz_trace_replay_001"; }
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
        snprintf(path, sizeof(path), "../actual/si_transfer_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/si_transfer_rvz_trace_replay_001.bin");
    }
    return path;
}
#
void gc_scenario_run(GcRam *ram) {
    // Inputs (from retail RVZ trace).
    u32 chan = must_u32("GC_TRACE_CHAN");
    u32 out_ptr = must_u32("GC_TRACE_OUT_PTR");
    u32 out_bytes = must_u32("GC_TRACE_OUT_BYTES");
    u32 in_ptr = must_u32("GC_TRACE_IN_PTR");
    u32 in_bytes = must_u32("GC_TRACE_IN_BYTES");
    u32 cb_ptr = must_u32("GC_TRACE_CB_PTR");
    u64 delay = must_u64("GC_TRACE_DELAY64");
    u64 sys_time_seed = must_u64("GC_TRACE_SYSTEM_TIME_SEED");
    u32 setalarm_delta = must_u32("GC_TRACE_OSSETALARM_DELTA");
    u32 hw_xfer_ok = must_u32("GC_TRACE_HW_XFER_OK");
#
    const char *in_outbuf = getenv("GC_TRACE_IN_OUTBUF");
    const char *in_type = getenv("GC_TRACE_IN_TYPE_DATA");
    const char *in_si_core = getenv("GC_TRACE_IN_SI_CORE");
    const char *in_inputbuf = getenv("GC_TRACE_IN_INPUTBUF");
    if (!in_outbuf || !in_type || !in_si_core || !in_inputbuf) die("missing input paths");
#
    // Seed memory windows exactly as observed in retail.
    u8 buf_out[0x20];
    u8 buf_type[0x10];
    u8 buf_si[0x160];
    u8 buf_inp[0x40];
    must_read_exact(in_outbuf, buf_out, sizeof(buf_out));
    must_read_exact(in_type, buf_type, sizeof(buf_type));
    must_read_exact(in_si_core, buf_si, sizeof(buf_si));
    must_read_exact(in_inputbuf, buf_inp, sizeof(buf_inp));
#
    // Place these bytes into the emulated RAM at the same addresses.
    u8 *p_out = gc_ram_ptr(ram, out_ptr, sizeof(buf_out));
    if (!p_out) die("gc_ram_ptr(out_ptr) failed");
    memcpy(p_out, buf_out, sizeof(buf_out));
#
    // Type array is fixed in retail (base 0x8013E0B4), but SITransfer can be
    // called with &Type[chan] (r6 varies). We seed the full 0x10 window.
    u8 *p_type = gc_ram_ptr(ram, 0x8013E0B4u, sizeof(buf_type));
    if (!p_type) die("gc_ram_ptr(type_data) failed");
    memcpy(p_type, buf_type, sizeof(buf_type));
#
    u8 *p_si = gc_ram_ptr(ram, 0x801A6F98u, sizeof(buf_si));
    if (!p_si) die("gc_ram_ptr(si_core) failed");
    memcpy(p_si, buf_si, sizeof(buf_si));
#
    u8 *p_inp = gc_ram_ptr(ram, 0x801A7148u, sizeof(buf_inp));
    if (!p_inp) die("gc_ram_ptr(inputbuf) failed");
    memcpy(p_inp, buf_inp, sizeof(buf_inp));
#
    // Seed deterministic system time for alarm scheduling.
    gc_os_set_system_time_seed(sys_time_seed);
    gc_os_set_setalarm_delta(setalarm_delta);
    gc_si_set_hw_xfer_ok(hw_xfer_ok);
#
    BOOL ret = SITransfer((s32)chan, (void *)(uintptr_t)out_ptr, out_bytes, (void *)(uintptr_t)in_ptr,
                          in_bytes, (void *)(uintptr_t)cb_ptr, delay);
#
    // Output layout for replay comparison:
    // 0x00 marker
    // 0x04 ret
    // 0x08 chan
    // 0x0C out_bytes
    // 0x10 in_bytes
    // 0x14 delay_hi
    // 0x18 delay_lo
    // 0x20 outbuf (0x20)
    // 0x40 type_data (0x10) [base 0x8013E0B4]
    // 0x50 si_core (0x160)  [base 0x801A6F98]
    // 0x1B0 inputbuf (0x40) [base 0x801A7148]
    u8 *out = gc_ram_ptr(ram, 0x80300000u, 0x1F0);
    if (!out) die("gc_ram_ptr(out) failed");
    memset(out, 0, 0x1F0);
#
    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, (u32)ret);
    wr32be(out + 0x08, chan);
    wr32be(out + 0x0C, out_bytes);
    wr32be(out + 0x10, in_bytes);
    wr32be(out + 0x14, (u32)(delay >> 32));
    wr32be(out + 0x18, (u32)(delay & 0xFFFFFFFFu));
#
    // Re-dump the same memory windows post-call.
    memcpy(out + 0x20, gc_ram_ptr(ram, out_ptr, 0x20), 0x20);
    memcpy(out + 0x40, gc_ram_ptr(ram, 0x8013E0B4u, 0x10), 0x10);
    memcpy(out + 0x50, gc_ram_ptr(ram, 0x801A6F98u, 0x160), 0x160);
    memcpy(out + 0x1B0, gc_ram_ptr(ram, 0x801A7148u, 0x40), 0x40);
}
