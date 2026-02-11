#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;

typedef int BOOL;

#include "../sdk_state.h"
#include "gc_mem.h"

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

// -----------------------------------------------------------------------------
// SITransfer (trace-replay driven)
//
// For MP4 we observe SITransfer used primarily by PAD init/status probing. The
// retail oracle shows it writing a SIPacket entry and, for some calls, arming
// an OSAlarm with fire = __OSGetSystemTime() + delay.
//
// We model only the memory-visible side effects needed by trace replay. This
// will be expanded as we collect more cases.
// -----------------------------------------------------------------------------

static inline void store_u32be(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static inline void store_u64be(uint32_t addr, uint64_t v) {
    store_u32be(addr + 0, (uint32_t)(v >> 32));
    store_u32be(addr + 4, (uint32_t)(v & 0xFFFFFFFFu));
}

static inline uint32_t load_u32be(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return 0;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint64_t load_u64be(uint32_t addr) {
    return ((uint64_t)load_u32be(addr) << 32) | (uint64_t)load_u32be(addr + 4);
}

// Deterministic seed used by SITransfer's alarm scheduling in host scenarios.
// The trace replay harness derives this from the retail alarm fire time.
static u64 gc_os_system_time_seed;
void gc_os_set_system_time_seed(u64 system_time) { gc_os_system_time_seed = system_time; }
static u32 gc_os_setalarm_delta;
void gc_os_set_setalarm_delta(u32 delta_ticks) { gc_os_setalarm_delta = delta_ticks; }
static u32 gc_si_hw_xfer_ok;
void gc_si_set_hw_xfer_ok(u32 ok) { gc_si_hw_xfer_ok = ok ? 1u : 0u; }

// Retail SI globals (MP4 GMPE01_00 symbols):
// - Packet base:     0x801A6F98 (4 * 0x20)
// - Alarm base:      0x801A7018 (4 * 0x28) within our si_core dump at +0x80
//
// Note: These are *SDK internal* globals, not hardware registers. We write
// big-endian words so the host RAM dumps match Dolphin dumps byte-for-byte.
enum {
    SI_PACKET_BASE = 0x801A6F98u,
    SI_PACKET_SIZE = 0x20u,
    SI_ALARM_BASE = 0x801A7018u,
    SI_ALARM_SIZE = 0x28u,
    SI_XFER_TIME_BASE = 0x801A70D8u, // u64[4]
    // Observed alarm handler pointer in MP4 retail traces.
    SI_ALARM_HANDLER_PC = 0x800D9C38u,
    // Observed queue pointer written into the alarm in MP4 traces.
    OS_ALARM_QUEUE_PTR = 0x801A5E68u,
};

BOOL SITransfer(s32 chan, void *output, u32 outputBytes, void *input, u32 inputBytes, void *callback, u64 delay) {
    if (chan < 0 || chan >= 4) return 0;

    // Retail behavior (SIBios.c):
    // - now = __OSGetSystemTime()
    // - fire = (delay == 0) ? now : (XferTime[chan] + delay)
    // - if (now < fire): OSSetAlarm(&Alarm[chan], fire-now, AlarmHandler)
    // - else if (__SITransfer(...) succeeds): return TRUE
    // - queue packet fields + packet->fire = fire; return TRUE
    u64 now = gc_os_system_time_seed;
    u64 fire = now;
    if (delay != 0) {
        u64 xfer = load_u64be(SI_XFER_TIME_BASE + (uint32_t)chan * 8u);
        fire = xfer + delay;
    }

    // If we are at/after fire time, retail attempts an immediate hardware
    // transfer via __SITransfer(). If it succeeds, SITransfer returns TRUE
    // without queuing a packet or touching alarms.
    //
    // We don't emulate SI hardware yet; the trace replay harness seeds whether
    // the immediate transfer succeeded for the observed case.
    if (now >= fire && gc_si_hw_xfer_ok) {
        return 1;
    }

    uint32_t out_ptr = (uint32_t)(uintptr_t)output;
    uint32_t in_ptr = (uint32_t)(uintptr_t)input;
    uint32_t cb_ptr = (uint32_t)(uintptr_t)callback;

    // Queue SIPacket[chan] fields (8x u32 BE).
    const uint32_t pkt = SI_PACKET_BASE + (uint32_t)chan * SI_PACKET_SIZE;
    store_u32be(pkt + 0x00, (uint32_t)chan);
    store_u32be(pkt + 0x04, out_ptr);
    store_u32be(pkt + 0x08, outputBytes);
    store_u32be(pkt + 0x0C, in_ptr);
    store_u32be(pkt + 0x10, inputBytes);
    store_u32be(pkt + 0x14, cb_ptr);
    store_u64be(pkt + 0x18, fire);

    // Arm OSAlarm[chan] when now < fire.
    if (now < fire) {
        const uint32_t alarm = SI_ALARM_BASE + (uint32_t)chan * SI_ALARM_SIZE;

        // Trace-replay-driven layout (see tools/replay_trace_case_si_transfer.sh).
        store_u32be(alarm + 0x00, SI_ALARM_HANDLER_PC);
        store_u32be(alarm + 0x04, 0);
        // In retail, OSSetAlarm reads system time again internally; alarm->fire
        // can be slightly greater than packet->fire. Replay harness seeds the
        // observed delta as gc_os_setalarm_delta.
        store_u64be(alarm + 0x08, fire + (u64)gc_os_setalarm_delta);
        store_u32be(alarm + 0x10, 0);
        store_u32be(alarm + 0x14, OS_ALARM_QUEUE_PTR);
    }

    return 1;
}

// -----------------------------------------------------------------------------
// SIGetResponse (retail trace-replay driven)
//
// Retail behavior (SIBios.c):
// - SIGetResponseRaw(chan) reads SIGetStatus(chan) and, if SI_ERROR_RDST is set,
//   pulls two u32 words from __SIRegs into InputBuffer[chan] and sets
//   InputBufferValid[chan] = TRUE.
// - SIGetResponse() always calls SIGetResponseRaw(), then:
//     rc = InputBufferValid[chan];
//     InputBufferValid[chan] = FALSE;
//     if (rc) copy InputBuffer words to `data`.
//
// For host trace replay we cannot read MMIO (__SIRegs). Instead we seed:
// - per-channel status word (used to model SIGetStatus(chan))
// - per-channel response words (what __SIRegs would have returned)
//
// These seeds are stored in GC_SDK_STATE (sdk_state.h) and are derived by the
// replay script from retail entry/exit snapshots.
// -----------------------------------------------------------------------------

enum {
    // From dolphin/os/OSSerial.h in the SDK:
    //   #define SI_ERROR_RDST 0x0020
    SI_ERROR_RDST = 0x0020u,

    // Retail MP4 SDK internal globals (GMPE01_00 symbols):
    SI_INPUTBUF_VALID_BASE = 0x801A7148u, // BOOL[4]
    SI_INPUTBUF_BASE = 0x801A7158u,       // u32[4][2]
    SI_INPUTBUF_VCOUNT_BASE = 0x801A7178u // vu32[4]
};

// Host-side deterministic seeds for SIGetResponseRaw().
// We mirror these into RAM-backed sdk_state for observability, but the source of
// truth is these arrays to avoid depending on unrelated memory writes.
static u32 gc_si_status_seed_arr[4];
static u32 gc_si_resp_seed_arr[4][2];

void gc_si_set_status_seed(u32 chan, u32 status) {
    if (chan >= 4) return;
    gc_si_status_seed_arr[chan] = status;
    if (getenv("GC_TRACE_DEBUG")) {
        fprintf(stderr, "[gc_si_set_status_seed] chan=%u status=0x%08X\n", (unsigned)chan, (unsigned)status);
    }
    gc_sdk_state_store_u32be(GC_SDK_OFF_SI_STATUS_BASE + chan * 4u, status);
}

void gc_si_set_resp_words_seed(u32 chan, u32 word0, u32 word1) {
    if (chan >= 4) return;
    gc_si_resp_seed_arr[chan][0] = word0;
    gc_si_resp_seed_arr[chan][1] = word1;
    if (getenv("GC_TRACE_DEBUG")) {
        fprintf(stderr, "[gc_si_set_resp_words_seed] chan=%u w0=0x%08X w1=0x%08X\n", (unsigned)chan, (unsigned)word0, (unsigned)word1);
    }
    const u32 off = GC_SDK_OFF_SI_RESP_WORDS_BASE + chan * 8u;
    gc_sdk_state_store_u32be(off + 0, word0);
    gc_sdk_state_store_u32be(off + 4, word1);
}

static BOOL SIGetResponseRaw(s32 chan) {
    if (chan < 0 || chan >= 4) return 0;

    // Model SIGetStatus(chan) using a deterministic per-channel seed.
    u32 sr_addr = GC_SDK_STATE_BASE + GC_SDK_OFF_SI_STATUS_BASE + (u32)chan * 4u;
    u32 sr = gc_si_status_seed_arr[(u32)chan];
    if (getenv("GC_TRACE_DEBUG")) {
        fprintf(stderr, "[SIGetResponseRaw] base=0x%08X status_off=0x%X addr=0x%08X\n",
                (unsigned)GC_SDK_STATE_BASE, (unsigned)GC_SDK_OFF_SI_STATUS_BASE, (unsigned)sr_addr);
        fprintf(stderr, "[SIGetResponseRaw] chan=%d sr=0x%08X\n", (int)chan, sr);
    }
    if (sr & SI_ERROR_RDST) {
        // Model __SIRegs words using deterministic seeds.
        u32 w0 = gc_si_resp_seed_arr[(u32)chan][0];
        u32 w1 = gc_si_resp_seed_arr[(u32)chan][1];

        store_u32be(SI_INPUTBUF_BASE + (u32)chan * 8u + 0, w0);
        store_u32be(SI_INPUTBUF_BASE + (u32)chan * 8u + 4, w1);
        store_u32be(SI_INPUTBUF_VALID_BASE + (u32)chan * 4u, 1);
        // Debug invariant: the write must be observable immediately.
        // If this trips, our host RAM mapping is wrong.
        u32 vchk = load_u32be(SI_INPUTBUF_VALID_BASE + (u32)chan * 4u);
        if (getenv("GC_TRACE_DEBUG")) {
            fprintf(stderr, "[SIGetResponseRaw] wrote valid=1, readback=0x%08X\n", vchk);
        }
        if (vchk != 1u) {
            // No OSReport here: keep this deterministic and fail-fast in scenarios.
            return 0;
        }

        // Note: InputBufferVcount is driven by the polling interrupt handler,
        // not SIGetResponse(). Leave it untouched for now; if retail traces
        // show changes, we'll expand the model.
        (void)SI_INPUTBUF_VCOUNT_BASE;

        return 1;
    }
    return 0;
}

BOOL SIGetResponse(s32 chan, void *data) {
    if (chan < 0 || chan >= 4) return 0;

    BOOL enabled = OSDisableInterrupts();

    SIGetResponseRaw(chan);

    u32 valid = load_u32be(SI_INPUTBUF_VALID_BASE + (u32)chan * 4u);
    store_u32be(SI_INPUTBUF_VALID_BASE + (u32)chan * 4u, 0);

    if (valid) {
        u32 w0 = load_u32be(SI_INPUTBUF_BASE + (u32)chan * 8u + 0);
        u32 w1 = load_u32be(SI_INPUTBUF_BASE + (u32)chan * 8u + 4);
        u32 dst = (u32)(uintptr_t)data;
        store_u32be(dst + 0, w0);
        store_u32be(dst + 4, w1);
    }

    OSRestoreInterrupts(enabled);
    return valid ? 1 : 0;
}
