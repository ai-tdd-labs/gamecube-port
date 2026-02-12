#include <stdint.h>

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int BOOL;
typedef int8_t s8;
typedef uint8_t u8;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

// RAM-backed state (big-endian in MEM1) for dump comparability.
#include "../sdk_state.h"

u32 gc_pad_initialized;
u32 gc_pad_si_refresh_calls;
u32 gc_pad_register_reset_calls;
u32 gc_pad_reset_mask;
u32 gc_pad_cmd_probe_device[4];
u32 gc_pad_spec;
u32 gc_pad_fix_bits;
u16 gc_os_wireless_pad_fix_mode;
u32 gc_pad_motor_cmd[4];

static u32 RecalibrateBits;
static u32 ResettingBits;
static s32 ResettingChan = 32;
static u32 ResetCallbackPtr;

#define SI_MAX_CHAN 4
#define PAD_CHAN0_BIT 0x80000000u
#define PAD_CHAN1_BIT 0x40000000u
#define PAD_CHAN2_BIT 0x20000000u
#define PAD_CHAN3_BIT 0x10000000u

// Minimal SDK-visible status struct used by MP4 pad.c.
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
} PADStatus;

enum {
    PAD_ERR_NONE = 0,
    PAD_ERR_NO_CONTROLLER = -1,
    PAD_ERR_NOT_READY = -2,
};

enum {
    GC_PAD_MAKE_STATUS_SPEC0 = 0,
    GC_PAD_MAKE_STATUS_SPEC1 = 1,
    GC_PAD_MAKE_STATUS_SPEC2 = 2,
};

static u32 gc_pad_make_status_kind;

// Retail MP4 global locations (from external/mp4-decomp/config/GMPE01_00/symbols.txt).
// We mirror a small subset so retail trace replays can assert real memory side effects.
#define PAD_PADSPEC_SHADOW_ADDR 0x801D450Cu // __PADSpec (always cleared by PADSetSpec)
#define PAD_SPEC_ADDR 0x801D3924u          // Spec (the actual current spec)
#define PAD_MAKE_STATUS_ADDR 0x801D3928u   // MakeStatus function pointer
#define PAD_SPEC0_MAKE_STATUS_PC 0x800C5038u
#define PAD_SPEC1_MAKE_STATUS_PC 0x800C51ACu
#define PAD_SPEC2_MAKE_STATUS_PC 0x800C5320u

static inline void store_u32be(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

// Exposed in the SDK.
// Decomp behavior: set a MakeStatus function pointer based on spec (we persist a
// stable "kind" token for deterministic dumps) and store Spec.
void PADSetSpec(u32 spec) {
    u32 kind = GC_PAD_MAKE_STATUS_SPEC2;
    u32 make_status_pc = PAD_SPEC2_MAKE_STATUS_PC;
    switch (spec) {
        case 0: /* PAD_SPEC_0 */
            kind = GC_PAD_MAKE_STATUS_SPEC0;
            make_status_pc = PAD_SPEC0_MAKE_STATUS_PC;
            break;
        case 1: /* PAD_SPEC_1 */
            kind = GC_PAD_MAKE_STATUS_SPEC1;
            make_status_pc = PAD_SPEC1_MAKE_STATUS_PC;
            break;
        default: /* PAD_SPEC_2..5 */
            kind = GC_PAD_MAKE_STATUS_SPEC2;
            make_status_pc = PAD_SPEC2_MAKE_STATUS_PC;
            break;
    }

    gc_pad_spec = spec;
    gc_pad_make_status_kind = kind;
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_SPEC, spec);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND, kind);

    // Retail-visible globals for trace replay (mirror Pad.c behavior):
    // - __PADSpec is always cleared
    // - Spec is set to the requested value
    // - MakeStatus points to the selected SPECx_MakeStatus implementation
    store_u32be(PAD_PADSPEC_SHADOW_ADDR, 0);
    store_u32be(PAD_SPEC_ADDR, spec);
    store_u32be(PAD_MAKE_STATUS_ADDR, make_status_pc);
}

u32 PADGetSpec(void) { return gc_pad_spec; }
static u64 OSGetTime(void) { return 0; }
// SI module (sdk_port). PADInit calls this during init.
void SIRefreshSamplingRate(void);
static void PAD_SIRefreshSamplingRate(void) { gc_pad_si_refresh_calls++; SIRefreshSamplingRate(); }
static void OSRegisterResetFunction(void *info) { (void)info; gc_pad_register_reset_calls++; }
static BOOL PADResetInternal(u32 mask) { gc_pad_reset_mask = mask; return TRUE; }

// --- SDK surface area used by MP4's pad.c ---
//
// These are currently modeled as deterministic, side-effect-recording stubs.
// Correctness is enforced by dedicated unit tests (DOL expected vs host actual)
// for the specific observable state each game callsite relies on.

u32 PADRead(PADStatus *status) {
    // Deterministic stub for MP4 init:
    // Retail trace shows chan0 present+idle (err=0) and chans 1..3 absent (err=-1),
    // and return value PAD_CHAN0_BIT (0x80000000).
    if (status) {
        for (int i = 0; i < 4; i++) {
            status[i].button = 0;
            status[i].stickX = 0;
            status[i].stickY = 0;
            status[i].substickX = 0;
            status[i].substickY = 0;
            status[i].triggerL = 0;
            status[i].triggerR = 0;
            status[i].analogA = 0;
            status[i].analogB = 0;
            status[i].err = (s8)((i == 0) ? PAD_ERR_NONE : PAD_ERR_NO_CONTROLLER);
        }
    }
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_READ_CALLS,
                             gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_READ_CALLS, 0) + 1u);
    return PAD_CHAN0_BIT;
}

/* Mirror decomp Padclamp.c ClampStick/ClampTrigger + PADClamp. */

typedef struct PADClampRegion {
    u8 minTrigger;
    u8 maxTrigger;
    s8 minStick;
    s8 maxStick;
    s8 xyStick;
    s8 minSubstick;
    s8 maxSubstick;
    s8 xySubstick;
} PADClampRegion;

static PADClampRegion ClampRegion = {
    30, 180,        /* triggers */
    15, 72, 40,     /* left stick */
    15, 59, 31,     /* right stick */
};

static void ClampStick(s8 *px, s8 *py, s8 max, s8 xy, s8 min) {
    int x = *px;
    int y = *py;
    int signX, signY, d;

    if (0 <= x) { signX = 1; } else { signX = -1; x = -x; }
    if (0 <= y) { signY = 1; } else { signY = -1; y = -y; }
    if (x <= min) { x = 0; } else { x -= min; }
    if (y <= min) { y = 0; } else { y -= min; }

    if (x == 0 && y == 0) { *px = *py = 0; return; }

    if (xy * y <= xy * x) {
        d = xy * x + (max - xy) * y;
        if (xy * max < d) { x = (s8)(xy * max * x / d); y = (s8)(xy * max * y / d); }
    } else {
        d = xy * y + (max - xy) * x;
        if (xy * max < d) { x = (s8)(xy * max * x / d); y = (s8)(xy * max * y / d); }
    }
    *px = (s8)(signX * x);
    *py = (s8)(signY * y);
}

#define PAD_CHANMAX 4

void PADClamp(PADStatus *status) {
    int i;
    for (i = 0; i < PAD_CHANMAX; i++, status++) {
        if (status->err != PAD_ERR_NONE) continue;
        ClampStick(&status->stickX, &status->stickY,
                   ClampRegion.maxStick, ClampRegion.xyStick, ClampRegion.minStick);
        ClampStick(&status->substickX, &status->substickY,
                   ClampRegion.maxSubstick, ClampRegion.xySubstick, ClampRegion.minSubstick);
        if (status->triggerL <= ClampRegion.minTrigger) {
            status->triggerL = 0;
        } else {
            if (ClampRegion.maxTrigger < status->triggerL)
                status->triggerL = ClampRegion.maxTrigger;
            status->triggerL -= ClampRegion.minTrigger;
        }
        if (status->triggerR <= ClampRegion.minTrigger) {
            status->triggerR = 0;
        } else {
            if (ClampRegion.maxTrigger < status->triggerR)
                status->triggerR = ClampRegion.maxTrigger;
            status->triggerR -= ClampRegion.minTrigger;
        }
    }
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_CLAMP_CALLS,
                             gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_CLAMP_CALLS, 0) + 1u);
}

BOOL PADReset(u32 mask) {
    // Seed modeled internal globals from RAM-backed sdk_state so host trace-replay
    // scenarios can exactly mirror retail pre-state.
    ResettingBits = gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RESETTING_BITS, ResettingBits);
    ResettingChan = (s32)gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RESETTING_CHAN, (u32)ResettingChan);
    RecalibrateBits = gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RECALIBRATE_BITS, RecalibrateBits);
    ResetCallbackPtr = gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RESET_CB_PTR, ResetCallbackPtr);

    gc_pad_reset_mask = mask;
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_MASK, mask);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_CALLS,
                             gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RESET_CALLS, 0) + 1u);

    // Minimal modeled behavior for retail MP4 snapshots:
    // The real SDK queues a reset by OR-ing ResettingBits, then immediately runs
    // one DoReset() step which:
    // - sets ResettingChan = cntlzw(ResettingBits)
    // - clears the corresponding bit from ResettingBits
    // We don't emulate the async SITransfer, only these observable globals.
    ResettingBits |= mask;
    ResettingChan = (ResettingBits == 0) ? 32 : (s32)__builtin_clz(ResettingBits);
    if (ResettingChan != 32) {
        u32 chanBit = PAD_CHAN0_BIT >> (u32)ResettingChan;
        ResettingBits &= ~chanBit;
    }
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_BITS, ResettingBits);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESETTING_CHAN, (u32)ResettingChan);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, RecalibrateBits);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RESET_CB_PTR, ResetCallbackPtr);
    return TRUE;
}

BOOL PADRecalibrate(u32 mask) {
    RecalibrateBits = mask;
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_MASK, mask);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_CALLS,
                             gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_RECALIBRATE_CALLS, 0) + 1u);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_RECALIBRATE_BITS, RecalibrateBits);
    return TRUE;
}

void PADControlMotor(s32 chan, u32 command) {
    if (chan < 0 || chan >= 4) return;
    gc_pad_motor_cmd[chan] = command;
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MOTOR_CMD_BASE + (uint32_t)chan * 4u, command);
}

BOOL PADInit(void) {
    s32 chan;
    gc_pad_initialized = gc_sdk_state_load_u32_or(GC_SDK_OFF_PAD_INITIALIZED, gc_pad_initialized);
    if (gc_pad_initialized) {
        return TRUE;
    }

    if (gc_pad_spec) {
        PADSetSpec(gc_pad_spec);
    }

    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_PAD_INITIALIZED, &gc_pad_initialized, 1u);

    if (gc_pad_fix_bits != 0) {
        u64 time = OSGetTime();
        gc_os_wireless_pad_fix_mode = (u16)((((time)&0xffff) + ((time >> 16) & 0xffff) +
            ((time >> 32) & 0xffff) + ((time >> 48) & 0xffff)) & 0x3fffu);
        RecalibrateBits = PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT;
    }

    for (chan = 0; chan < SI_MAX_CHAN; ++chan) {
        gc_pad_cmd_probe_device[chan] = (0x4D << 24) | (chan << 22) | ((gc_os_wireless_pad_fix_mode & 0x3fffu) << 8);
    }

    PAD_SIRefreshSamplingRate();
    OSRegisterResetFunction(0);

    return PADResetInternal(PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
}
