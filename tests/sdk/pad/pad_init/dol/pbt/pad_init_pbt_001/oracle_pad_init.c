#include <stdint.h>

#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#endif

#define SI_MAX_CHAN 4
#define PAD_CHAN0_BIT 0x80000000u
#define PAD_CHAN1_BIT 0x40000000u
#define PAD_CHAN2_BIT 0x20000000u
#define PAD_CHAN3_BIT 0x10000000u

#define PAD_PADSPEC_SHADOW_ADDR 0x801D450Cu
#define PAD_SPEC_ADDR 0x801D3924u
#define PAD_MAKE_STATUS_ADDR 0x801D3928u
#define PAD_SPEC0_MAKE_STATUS_PC 0x800C5038u
#define PAD_SPEC1_MAKE_STATUS_PC 0x800C51ACu
#define PAD_SPEC2_MAKE_STATUS_PC 0x800C5320u

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
static u32 gc_pad_make_status_kind;

static inline void store_u32be_addr(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4u);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

void PADSetSpec(u32 spec) {
    u32 kind = 2u;
    u32 make_status_pc = PAD_SPEC2_MAKE_STATUS_PC;
    if (spec == 0u) {
        kind = 0u;
        make_status_pc = PAD_SPEC0_MAKE_STATUS_PC;
    } else if (spec == 1u) {
        kind = 1u;
        make_status_pc = PAD_SPEC1_MAKE_STATUS_PC;
    }

    gc_pad_spec = spec;
    gc_pad_make_status_kind = kind;
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_SPEC, spec);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND, kind);
    store_u32be_addr(PAD_PADSPEC_SHADOW_ADDR, 0u);
    store_u32be_addr(PAD_SPEC_ADDR, spec);
    store_u32be_addr(PAD_MAKE_STATUS_ADDR, make_status_pc);
}

static u64 OSGetTime(void) { return 0; }
static void PAD_SIRefreshSamplingRate(void) { gc_pad_si_refresh_calls++; }
static void OSRegisterResetFunction(void *info) { (void)info; gc_pad_register_reset_calls++; }
static BOOL PADResetInternal(u32 mask) { gc_pad_reset_mask = mask; return TRUE; }

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

    if (gc_pad_fix_bits != 0u) {
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
