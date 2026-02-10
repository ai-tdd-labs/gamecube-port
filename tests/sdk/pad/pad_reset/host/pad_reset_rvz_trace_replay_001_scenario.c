#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_state.h"

typedef uint32_t u32;
typedef int BOOL;

BOOL PADReset(u32 mask);

static u32 must_u32(const char *key) {
    const char *s = getenv(key);
    if (!s || !*s) die("missing env var");
    char *endp = 0;
    u32 v = (u32)strtoul(s, &endp, 0);
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

static uint32_t be32_load(const uint8_t b[4]) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

const char *gc_scenario_label(void) { return "PADReset/rvz_trace_replay_001"; }

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
        snprintf(path, sizeof(path), "../actual/pad_reset_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/pad_reset_rvz_trace_replay_001.bin");
    }
    return path;
}

void gc_scenario_run(GcRam *ram) {
    // Runner dumps 0x80300000..0x80300040.
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!out) die("gc_ram_ptr(out) failed");
    memset(out, 0, 0x40);

    u32 mask = must_u32("GC_TRACE_MASK");

    const char *in_rb = getenv("GC_TRACE_IN_RESETTING_BITS");
    const char *in_rc = getenv("GC_TRACE_IN_RESETTING_CHAN");
    const char *in_recal = getenv("GC_TRACE_IN_RECAL_BITS");
    const char *in_rcb = getenv("GC_TRACE_IN_RESET_CB");
    const char *in_ptype = getenv("GC_TRACE_IN_PAD_TYPE");
    if (!in_rb || !in_rc || !in_recal || !in_rcb || !in_ptype) die("missing input paths");

    uint8_t rb_b[4], rc_b[4], recal_b[4], rcb_b[4];
    uint8_t pad_type[0x20];
    must_read_exact(in_rb, rb_b, 4);
    must_read_exact(in_rc, rc_b, 4);
    must_read_exact(in_recal, recal_b, 4);
    must_read_exact(in_rcb, rcb_b, 4);
    must_read_exact(in_ptype, pad_type, sizeof(pad_type));

    // Inputs are retail big-endian words.
    uint32_t rb_be = be32_load(rb_b);
    uint32_t rc_be = be32_load(rc_b);
    uint32_t recal_be = be32_load(recal_b);
    uint32_t rcb_be = be32_load(rcb_b);

    // Seed modeled PAD internal state into sdk_state (RAM-backed).
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_BITS, rb_be);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_CHAN, rc_be);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, recal_be);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_CB_PTR, rcb_be);

    BOOL ret = PADReset(mask);

    // Capture modeled output state from sdk_state.
    uint32_t out_rb = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESETTING_BITS);
    uint32_t out_rc = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESETTING_CHAN);
    uint32_t out_recal = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS);
    uint32_t out_rcb = gc_sdk_state_load_u32be(GC_SDK_OFF_PAD_RESET_CB_PTR);

    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, (uint32_t)ret);
    wr32be(out + 0x10, out_rb);
    wr32be(out + 0x14, out_rc);
    wr32be(out + 0x18, out_recal);
    wr32be(out + 0x1C, out_rcb);
    memcpy(out + 0x20, pad_type, sizeof(pad_type)); // unchanged in observed retail cases
}
