#include <stdint.h>

#include "gc_mem.h"

// Host workload stub for MP4's pfDrawFonts().
//
// This is game-specific (not Nintendo SDK). We do not emulate GX rendering here.
// Purpose: keep the MP4 mainloop workload progressing while leaving a deterministic
// breadcrumb that the call happened.
//
// Writes:
// - 0x80300020: call count (big-endian u32)
// - 0x80300024: 'FONT' marker (big-endian u32)
static uint32_t s_calls;

static inline void wr32be_mem(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

void pfDrawFonts(void) {
    s_calls++;
    wr32be_mem(0x80300020u, s_calls);
    wr32be_mem(0x80300024u, 0x464F4E54u); // 'FONT'
}

