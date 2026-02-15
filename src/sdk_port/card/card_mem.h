#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../gc_mem.h"

// CARD code in this repo sometimes stores pointers as:
// - raw host pointers (from gc_ram_ptr() in host scenarios), or
// - GC virtual addresses (0x80xxxxxx) when mirroring PPC-side behavior.
//
// This helper makes both representations usable without breaking existing suites.
static inline void* card_ptr(uintptr_t addr, size_t len)
{
    // MEM1 window (covers all addresses our suites use).
    if ((uint32_t)addr >= 0x80000000u && (uint32_t)addr < 0x81800000u) {
        return gc_mem_ptr((uint32_t)addr, len);
    }
    return (void*)addr;
}

static inline const void* card_ptr_ro(uintptr_t addr, size_t len)
{
    return (const void*)card_ptr(addr, len);
}

