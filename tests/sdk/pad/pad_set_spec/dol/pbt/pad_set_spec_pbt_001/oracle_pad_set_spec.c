#include <stdint.h>

#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;

static u32 s_pad_spec;

#define PAD_PADSPEC_SHADOW_ADDR 0x801D450Cu
#define PAD_SPEC_ADDR 0x801D3924u
#define PAD_MAKE_STATUS_ADDR 0x801D3928u
#define PAD_SPEC0_MAKE_STATUS_PC 0x800C5038u
#define PAD_SPEC1_MAKE_STATUS_PC 0x800C51ACu
#define PAD_SPEC2_MAKE_STATUS_PC 0x800C5320u

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

    s_pad_spec = spec;
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_SPEC, spec);
    gc_sdk_state_store_u32be(GC_SDK_OFF_PAD_MAKE_STATUS_KIND, kind);
    store_u32be_addr(PAD_PADSPEC_SHADOW_ADDR, 0u);
    store_u32be_addr(PAD_SPEC_ADDR, s_pad_spec);
    store_u32be_addr(PAD_MAKE_STATUS_ADDR, make_status_pc);
}
