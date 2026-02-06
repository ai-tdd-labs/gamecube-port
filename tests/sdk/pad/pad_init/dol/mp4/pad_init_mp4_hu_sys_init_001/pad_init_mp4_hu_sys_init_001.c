#ifdef GC_HOST_TEST
#include <stdint.h>
#include "runtime/pad.h"
extern uint8_t *gc_test_ram;
extern uint32_t gc_pad_initialized;
extern uint32_t gc_pad_si_refresh_calls;
extern uint32_t gc_pad_register_reset_calls;
extern uint32_t gc_pad_reset_mask;
extern uint32_t gc_pad_cmd_probe_device[4];
extern uint32_t gc_pad_spec;
extern uint32_t gc_pad_fix_bits;
extern uint16_t gc_os_wireless_pad_fix_mode;
#define RESULT_PTR() ((volatile uint8_t *)(gc_test_ram + 0x00300000))
typedef uint32_t u32;
static inline void store_be32(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}
#else
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define RESULT_PTR() ((volatile u32 *)0x80300000)

#define SI_MAX_CHAN 4
#define PAD_CHAN0_BIT 0x80000000u
#define PAD_CHAN1_BIT 0x40000000u
#define PAD_CHAN2_BIT 0x20000000u
#define PAD_CHAN3_BIT 0x10000000u

typedef struct OSResetFunctionInfo {
    void *func;
    int priority;
} OSResetFunctionInfo;

void OSInit(void) {}

static BOOL Initialized;
static u32 __PADSpec;
static u32 __PADFixBits;
static u16 __OSWirelessPadFixMode;
static u32 RecalibrateBits;
static u32 CmdProbeDevice[SI_MAX_CHAN];

static u32 si_refresh_calls = 0;
static u32 register_reset_calls = 0;
static u32 pad_reset_mask = 0;

void PADSetSpec(u32 spec) {
    (void)spec;
}

u64 OSGetTime(void) { return 0; }

void SIRefreshSamplingRate(void) { si_refresh_calls++; }

void OSRegisterResetFunction(OSResetFunctionInfo *info) {
    (void)info;
    register_reset_calls++;
}

BOOL PADReset(u32 mask) {
    pad_reset_mask = mask;
    return TRUE;
}

BOOL PADInit(void) {
    s32 chan;
    if (Initialized) {
        return TRUE;
    }

    if (__PADSpec) {
        PADSetSpec(__PADSpec);
    }

    Initialized = TRUE;

    if (__PADFixBits != 0) {
        u64 time = OSGetTime();
        __OSWirelessPadFixMode = (u16)((((time)&0xffff) + ((time >> 16) & 0xffff) +
            ((time >> 32) & 0xffff) + ((time >> 48) & 0xffff)) & 0x3fffu);
        RecalibrateBits = PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT;
    }

    for (chan = 0; chan < SI_MAX_CHAN; ++chan) {
        CmdProbeDevice[chan] = (0x4D << 24) | (chan << 22) | ((__OSWirelessPadFixMode & 0x3fffu) << 8);
    }

    SIRefreshSamplingRate();
    OSRegisterResetFunction(NULL);

    return PADReset(PAD_CHAN0_BIT | PAD_CHAN1_BIT | PAD_CHAN2_BIT | PAD_CHAN3_BIT);
}
#endif

int main(void) {
#ifndef GC_HOST_TEST
    OSInit();
    Initialized = FALSE;
    __PADSpec = 0;
    __PADFixBits = 0;
    __OSWirelessPadFixMode = 0;
    RecalibrateBits = 0;
    int i;
    for (i = 0; i < 4; ++i) {
        CmdProbeDevice[i] = 0;
    }
    si_refresh_calls = 0;
    register_reset_calls = 0;
    pad_reset_mask = 0;
#endif

#ifdef GC_HOST_TEST
    gc_pad_initialized = 0;
    gc_pad_spec = 0;
    gc_pad_fix_bits = 0;
    gc_os_wireless_pad_fix_mode = 0;
    gc_pad_si_refresh_calls = 0;
    gc_pad_register_reset_calls = 0;
    gc_pad_reset_mask = 0;
    for (int i = 0; i < 4; ++i) {
        gc_pad_cmd_probe_device[i] = 0;
    }
#endif

    PADInit();

#ifdef GC_HOST_TEST
    volatile uint8_t *result = RESULT_PTR();
    store_be32(result + 0x00, 0xDEADBEEF);
    store_be32(result + 0x04, gc_pad_initialized);
    store_be32(result + 0x08, gc_pad_si_refresh_calls);
    store_be32(result + 0x0C, gc_pad_register_reset_calls);
    store_be32(result + 0x10, gc_pad_reset_mask);
    store_be32(result + 0x14, gc_pad_cmd_probe_device[0]);
    store_be32(result + 0x18, gc_pad_cmd_probe_device[1]);
    store_be32(result + 0x1C, gc_pad_cmd_probe_device[2]);
    store_be32(result + 0x20, gc_pad_cmd_probe_device[3]);
#else
    volatile u32 *result = RESULT_PTR();
    result[0] = 0xDEADBEEF;
    result[1] = Initialized ? 1u : 0u;
    result[2] = si_refresh_calls;
    result[3] = register_reset_calls;
    result[4] = pad_reset_mask;
    result[5] = CmdProbeDevice[0];
    result[6] = CmdProbeDevice[1];
    result[7] = CmdProbeDevice[2];
    result[8] = CmdProbeDevice[3];
    while (1) {}
#endif
    return 0;
}
