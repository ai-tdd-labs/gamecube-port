#include <stdint.h>

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int BOOL;
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

#define SI_MAX_CHAN 4
#define PAD_CHAN0_BIT 0x80000000u
#define PAD_CHAN1_BIT 0x40000000u
#define PAD_CHAN2_BIT 0x20000000u
#define PAD_CHAN3_BIT 0x10000000u

// Exposed in the SDK. For now we only persist the chosen spec for callsite coverage.
void PADSetSpec(u32 spec) { gc_pad_spec = spec; }
static u64 OSGetTime(void) { return 0; }
// SI module (sdk_port). PADInit calls this during init.
void SIRefreshSamplingRate(void);
static void PAD_SIRefreshSamplingRate(void) { gc_pad_si_refresh_calls++; SIRefreshSamplingRate(); }
static void OSRegisterResetFunction(void *info) { (void)info; gc_pad_register_reset_calls++; }
static BOOL PADReset(u32 mask) { gc_pad_reset_mask = mask; return TRUE; }

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

    return PADReset(PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
}
