#pragma once

#include <stdint.h>

#include "gc_mem.h"

// Big-endian RAM-backed state storage for sdk_port.
//
// Rationale:
// - In Dolphin/PPC, globals live in MEM1 (big-endian).
// - On host, C globals live in process memory (little-endian) and won't appear
//   in MEM1 dumps.
// Storing SDK state in emulated RAM using explicit BE loads/stores makes dumps
// comparable across PPC and host runs.
//
// NOTE: This is intentionally minimal and grows subsystem-by-subsystem.

// Keep this outside the default arenas used by OSInit (0x80002000..0x81700000).
// Last 64 KiB of MEM1 (MEM1 end = 0x81800000).
#define GC_SDK_STATE_BASE 0x817F0000u
#define GC_SDK_STATE_SIZE 0x00010000u

enum {
  GC_SDK_STATE_MAGIC = 0x53444B53u, // "SDKS"
};

static inline uint8_t *gc_sdk_state_ptr(uint32_t off, uint32_t len) {
  return gc_mem_ptr(GC_SDK_STATE_BASE + off, len);
}

static inline void gc_sdk_state_store_u32be(uint32_t off, uint32_t v) {
  uint8_t *p = gc_sdk_state_ptr(off, 4);
  if (!p) return;
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)(v >> 0);
}

static inline uint32_t gc_sdk_state_load_u32be(uint32_t off) {
  uint8_t *p = gc_sdk_state_ptr(off, 4);
  if (!p) return 0;
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void gc_sdk_state_reset(void) {
  // Zero the whole page for determinism.
  uint8_t *p = gc_sdk_state_ptr(0, GC_SDK_STATE_SIZE);
  if (!p) return;
  for (uint32_t i = 0; i < GC_SDK_STATE_SIZE; i++) p[i] = 0;
  gc_sdk_state_store_u32be(0x00, GC_SDK_STATE_MAGIC);
}

// Offsets (grow as we migrate more subsystems)
enum {
  GC_SDK_OFF_MAGIC = 0x00,
  GC_SDK_OFF_OS_ARENA_LO = 0x10,
  GC_SDK_OFF_OS_ARENA_HI = 0x14,

  // OSAlloc / heap init metadata (host previously held these as globals).
  GC_SDK_OFF_OS_CURR_HEAP = 0x20,         // s32 (SDK global)
  GC_SDK_OFF_OSALLOC_HEAP_ARRAY = 0x24,   // u32
  GC_SDK_OFF_OSALLOC_NUM_HEAPS = 0x28,    // s32
  GC_SDK_OFF_OSALLOC_ARENA_START = 0x2C,  // u32
  GC_SDK_OFF_OSALLOC_ARENA_END = 0x30,    // u32
};
