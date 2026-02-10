#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef uint8_t u8;

// Mirror of Dolphin SDK PADStatus layout (see external/mp4-decomp/include/dolphin/pad.h).
typedef struct PADStatus {
    u16 button;
    s8 stickX;
    s8 stickY;
    s8 substickX;
    s8 substickY;
    u8 triggerL;
    u8 triggerR;
    u8 analogA;
    u8 analogB;
    s8 err;
    u8 _pad; // implicit padding in practice; make it explicit for byte-for-byte stability
} PADStatus;

u32 PADRead(PADStatus *status);

const char *gc_scenario_label(void) { return "PADRead/rvz_trace_replay_001"; }

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
        snprintf(path, sizeof(path), "../actual/pad_read_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/pad_read_rvz_trace_replay_001.bin");
    }
    return path;
}

void gc_scenario_run(GcRam *ram) {
    // Keep the output area reserved for the runner dump (0x80300000..0x80300040).
    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!out) die("gc_ram_ptr(out) failed");
    memset(out, 0, 0x40);

    // Use a separate scratch buffer for PADRead output so we don't overlap the dump window.
    PADStatus *st = (PADStatus *)gc_ram_ptr(ram, 0x80301000u, sizeof(PADStatus) * 4u);
    if (!st) die("gc_ram_ptr(status) failed");
    memset(st, 0, sizeof(PADStatus) * 4u);

    u32 ret = PADRead(st);

    // Pack oracle-friendly output:
    // [0x00] marker
    // [0x04] return value
    // [0x10..0x3F] 4*PADStatus bytes (48 bytes)
    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, ret);
    memcpy(out + 0x10, st, 48);
}

