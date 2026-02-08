#include <stdint.h>

// Minimal, host-buildable slice of MP4 decomp: HuPerfInit + HuPerfCreate from perf.c.
// Purpose: reachability/integration only (not a correctness oracle).

typedef uint8_t u8;
typedef int32_t s32;
typedef int64_t s64;
typedef uint16_t u16;

typedef struct OSStopwatch {
    char *name;
    uint64_t total;
    uint32_t hits;
    uint32_t running;
    uint64_t last;
    uint64_t min;
    uint64_t max;
} OSStopwatch;

// sdk_port provides these.
void OSInitStopwatch(OSStopwatch *sw, char *name);

typedef void (*GXDrawSyncCallback)(u16 token);
GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb);

typedef struct {
    u8 unk00;
    u8 unk01;
    u8 unk02;
    u8 unk03;
    char unk04[4];
    s64 unk08;
    s64 unk10;
    OSStopwatch unk18;
    int16_t unk50;
    char unk52[6];
} UnknownPerfStruct; // Size 0x58 (matches decomp comment)

static void DSCallbackFunc(u16 arg0);

static UnknownPerfStruct perf[10];
static uint32_t total_copy_clks;

// Forward decl (used by HuPerfInit).
s32 HuPerfCreate(char *arg0, u8 arg1, u8 arg2, u8 arg3, u8 arg4);

void HuPerfInit(void) {
    s32 i;

    for (i = 0; i < 10; i++) {
        perf[i].unk50 = 0;
    }
    // Mirrors MP4 decomp defaults.
    (void)HuPerfCreate("CPU", 0x00, 0xFF, 0x00, 0xFF);
    (void)HuPerfCreate("DRAW", 0xFF, 0x00, 0x00, 0xFF);
    (void)GXSetDrawSyncCallback(DSCallbackFunc);
    total_copy_clks = 0;
}

s32 HuPerfCreate(char *arg0, u8 arg1, u8 arg2, u8 arg3, u8 arg4) {
    s32 i;

    for (i = 0; i < 10; i++) {
        if (perf[i].unk50 == 0) {
            break;
        }
    }
    if (i == 10) {
        return -1;
    }

    OSInitStopwatch(&perf[i].unk18, arg0);
    perf[i].unk08 = 0;
    perf[i].unk50 = 1;
    perf[i].unk00 = arg1;
    perf[i].unk01 = arg2;
    perf[i].unk02 = arg3;
    perf[i].unk03 = arg4;
    return i;
}

static void DSCallbackFunc(u16 arg0) {
    // The full MP4 callback drives GPU metrics + stopwatch timings; we don't need that
    // to validate reachability. Keep it as a stub to satisfy GXSetDrawSyncCallback.
    (void)arg0;
}
