#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef int BOOL;

#include "../sdk_state.h"

// OS interrupt primitives (minimal sdk_port model).
int OSDisableInterrupts(void);
int OSRestoreInterrupts(int enabled);

// VI query (minimal sdk_port model lives in VI.c).
u32 VIGetTvFormat(void);

// Logging stub (from OSError.c).
int OSReport(const char *msg, ...);

// Mirror the SDK's XY tables (evidence: decomp_mario_party_4/src/dolphin/si/SISamplingRate.c).
typedef struct XY {
    u16 line;
    u8 count;
} XY;

static const XY XYNTSC[12] = {
    {263 - 17, 2}, {15, 18}, {30, 9}, {44, 6},  {52, 5},  {65, 4},
    {87, 3},       {87, 3},  {87, 3}, {131, 2}, {131, 2}, {131, 2},
};

static const XY XYPAL[12] = {
    {313 - 17, 2}, {15, 21}, {29, 11}, {45, 7},  {52, 6},  {63, 5},
    {78, 4},       {104, 3}, {104, 3}, {104, 3}, {104, 3}, {156, 2},
};

// VI formats (minimal).
enum {
    VI_NTSC = 0,
    VI_PAL = 1,
    VI_MPAL = 2,
    VI_EURGB60 = 5,
};

static u32 gc_si_sampling_rate;

static void SISetXY(u32 line, u8 count) {
    gc_sdk_state_store_u32be(GC_SDK_OFF_SI_SETXY_LINE, line);
    gc_sdk_state_store_u32be(GC_SDK_OFF_SI_SETXY_COUNT, (u32)count);
    u32 calls = gc_sdk_state_load_u32_or(GC_SDK_OFF_SI_SETXY_CALLS, 0);
    calls++;
    gc_sdk_state_store_u32be(GC_SDK_OFF_SI_SETXY_CALLS, calls);
}

void SISetSamplingRate(u32 msec) {
    const XY *xy;
    BOOL enabled;

    if (msec > 11) msec = 11;

    enabled = OSDisableInterrupts();
    gc_si_sampling_rate = msec;
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_SI_SAMPLING_RATE, &gc_si_sampling_rate, msec);

    switch (VIGetTvFormat()) {
    case VI_NTSC:
    case VI_MPAL:
    case VI_EURGB60:
        xy = XYNTSC;
        break;
    case VI_PAL:
        xy = XYPAL;
        break;
    default:
        OSReport("SISetSamplingRate: unknown TV format. Use default.");
        msec = 0;
        xy = XYNTSC;
        break;
    }

    // __VIRegs[54] & 1 selects 1x vs 2x multiplier (interlace state).
    u16 vi54 = gc_sdk_state_load_u16be_or(GC_SDK_OFF_VI_REGS_U16BE + (54u * 2u), 0);
    u32 factor = (vi54 & 1u) ? 2u : 1u;

    SISetXY(factor * (u32)xy[msec].line, xy[msec].count);
    OSRestoreInterrupts(enabled);
}

void SIRefreshSamplingRate(void) {
    gc_si_sampling_rate = gc_sdk_state_load_u32_or(GC_SDK_OFF_SI_SAMPLING_RATE, gc_si_sampling_rate);
    SISetSamplingRate(gc_si_sampling_rate);
}

