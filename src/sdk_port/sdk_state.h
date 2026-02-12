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

// Keep this at the end of MEM1, but small: retail games can push OS arena high
// very close to 0x81800000. We reserve just enough for our deterministic
// RAM-backed SDK state page.
//
// MEM1 end = 0x81800000, so this places the page at 0x817FE000..0x81800000.
#define GC_SDK_STATE_BASE 0x817FE000u
#define GC_SDK_STATE_SIZE 0x00002000u

enum {
  GC_SDK_STATE_MAGIC = 0x53444B53u, // "SDKS"
};

static inline uint8_t *gc_sdk_state_ptr(uint32_t off, uint32_t len) {
  return gc_mem_ptr(GC_SDK_STATE_BASE + off, len);
}

static inline int gc_sdk_state_mapped(uint32_t off, uint32_t len) {
  return gc_sdk_state_ptr(off, len) != 0;
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

static inline uint32_t gc_sdk_state_load_u32_or(uint32_t off, uint32_t fallback) {
  return gc_sdk_state_mapped(off, 4) ? gc_sdk_state_load_u32be(off) : fallback;
}

static inline void gc_sdk_state_store_u32_mirror(uint32_t off, uint32_t *mirror, uint32_t v) {
  if (mirror) *mirror = v;
  if (!gc_sdk_state_mapped(off, 4)) return;
  gc_sdk_state_store_u32be(off, v);
}

static inline uint16_t gc_sdk_state_load_u16be_or(uint32_t off, uint16_t fallback) {
  uint8_t *p = gc_sdk_state_ptr(off, 2);
  if (!p) return fallback;
  return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline void gc_sdk_state_store_u16be_mirror(uint32_t off, uint16_t *mirror, uint16_t v) {
  if (mirror) *mirror = v;
  uint8_t *p = gc_sdk_state_ptr(off, 2);
  if (!p) return;
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)(v >> 0);
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

  // OS interrupt state (minimal; enough for HuPadInit/SISetSamplingRate critical sections).
  GC_SDK_OFF_OS_INTS_ENABLED = 0x40,       // u32 (0/1)
  GC_SDK_OFF_OS_DISABLE_CALLS = 0x44,      // u32
  GC_SDK_OFF_OS_RESTORE_CALLS = 0x48,      // u32

  // VI state (subset sufficient for current deterministic tests + smoke snapshot).
  GC_SDK_OFF_VI_REGS_U16BE = 0x100,         // 64 * u16 = 0x80 bytes
  GC_SDK_OFF_VI_DISABLE_CALLS = 0x180,
  GC_SDK_OFF_VI_RESTORE_CALLS = 0x184,
  GC_SDK_OFF_VI_FLUSH_CALLS = 0x188,
  GC_SDK_OFF_VI_WAIT_RETRACE_CALLS = 0x18C,
  GC_SDK_OFF_VI_GET_NEXT_FIELD_CALLS = 0x190,
  GC_SDK_OFF_VI_GET_RETRACE_COUNT_CALLS = 0x194,
  GC_SDK_OFF_VI_SET_BLACK_CALLS = 0x198,
  GC_SDK_OFF_VI_NEXT_FIELD = 0x19C,
  GC_SDK_OFF_VI_RETRACE_COUNT = 0x1A0,
  GC_SDK_OFF_VI_BLACK = 0x1A4,
  GC_SDK_OFF_VI_TV_FORMAT = 0x1A8,

  GC_SDK_OFF_VI_CHANGE_MODE = 0x1C0,
  GC_SDK_OFF_VI_DISP_POS_X = 0x1C4,
  GC_SDK_OFF_VI_DISP_POS_Y = 0x1C8,
  GC_SDK_OFF_VI_DISP_SIZE_X = 0x1CC,
  GC_SDK_OFF_VI_DISP_SIZE_Y = 0x1D0,
  GC_SDK_OFF_VI_FB_SIZE_X = 0x1D4,
  GC_SDK_OFF_VI_FB_SIZE_Y = 0x1D8,
  GC_SDK_OFF_VI_XFB_MODE = 0x1DC,
  GC_SDK_OFF_VI_HELPER_CALLS = 0x1E0,
  GC_SDK_OFF_VI_NON_INTER = 0x1E4,

  GC_SDK_OFF_VI_PAN_POS_X = 0x1F0,
  GC_SDK_OFF_VI_PAN_POS_Y = 0x1F4,
  GC_SDK_OFF_VI_PAN_SIZE_X = 0x1F8,
  GC_SDK_OFF_VI_PAN_SIZE_Y = 0x1FC,

  // VI retrace callbacks (used by MP4 HuPadInit).
  GC_SDK_OFF_VI_POST_CB_PTR = 0x200,        // u32 (function pointer)
  GC_SDK_OFF_VI_POST_CB_SET_CALLS = 0x204,  // u32
  GC_SDK_OFF_VI_POST_CB_CALLS = 0x208,      // u32
  GC_SDK_OFF_VI_POST_CB_LAST_ARG = 0x20C,   // u32

  // DVD (minimal subset)
  GC_SDK_OFF_DVD_INITIALIZED = 0x300,
  GC_SDK_OFF_DVD_DRIVE_STATUS = 0x304,
  GC_SDK_OFF_DVD_OPEN_CALLS = 0x308,
  GC_SDK_OFF_DVD_READ_CALLS = 0x30C,
  GC_SDK_OFF_DVD_CLOSE_CALLS = 0x310,
  GC_SDK_OFF_DVD_LAST_READ_LEN = 0x314,
  GC_SDK_OFF_DVD_LAST_READ_OFF = 0x318,

  // PAD (minimal subset)
  GC_SDK_OFF_PAD_INITIALIZED = 0x320,
  GC_SDK_OFF_PAD_SPEC = 0x324,             // u32 (Spec)
  GC_SDK_OFF_PAD_MAKE_STATUS_KIND = 0x328, // u32 (0=SPEC0,1=SPEC1,2=SPEC2)

  // GX (minimal subset for MP4 init chain snapshot)
  GC_SDK_OFF_GX_CP_DISP_SRC = 0x340,
  GC_SDK_OFF_GX_CP_DISP_SIZE = 0x344,
  GC_SDK_OFF_GX_COPY_DISP_DEST = 0x348,

  // SI (minimal subset for HuPadInit callsite coverage)
  GC_SDK_OFF_SI_SAMPLING_RATE = 0x360,      // u32 (0..11)
  GC_SDK_OFF_SI_SETXY_LINE = 0x364,         // u32
  GC_SDK_OFF_SI_SETXY_COUNT = 0x368,        // u32 (low byte)
  GC_SDK_OFF_SI_SETXY_CALLS = 0x36C,        // u32

  // PAD read/maintenance call counters (required for linking MP4 pad.c workload).
  GC_SDK_OFF_PAD_READ_CALLS = 0x380,          // u32
  GC_SDK_OFF_PAD_CLAMP_CALLS = 0x384,         // u32
  GC_SDK_OFF_PAD_RESET_CALLS = 0x388,         // u32
  GC_SDK_OFF_PAD_RESET_MASK = 0x38C,          // u32
  GC_SDK_OFF_PAD_RECALIBRATE_CALLS = 0x390,   // u32
  GC_SDK_OFF_PAD_RECALIBRATE_MASK = 0x394,    // u32

  // PAD internal state (subset; enough to replay retail RVZ PADReset snapshots).
  GC_SDK_OFF_PAD_RESETTING_BITS = 0x398,      // u32
  GC_SDK_OFF_PAD_RESETTING_CHAN = 0x39C,      // u32 (0..32; 32 means none)
  GC_SDK_OFF_PAD_RECALIBRATE_BITS = 0x3A0,    // u32
  GC_SDK_OFF_PAD_RESET_CB_PTR = 0x3A4,        // u32 (function pointer)

  // PAD motor commands (minimal subset for HuPadInit)
  GC_SDK_OFF_PAD_MOTOR_CMD_BASE = 0x3B0,    // 4 * u32

  // VI pre-retrace callback (symmetric to post-retrace).
  GC_SDK_OFF_VI_PRE_CB_PTR = 0x210,         // u32 (function pointer)
  GC_SDK_OFF_VI_PRE_CB_SET_CALLS = 0x214,   // u32

  // SI trace-replay seeds (used by SIGetResponse retail replay).
  // These model the minimal MMIO-derived inputs needed to reproduce SIGetResponseRaw().
  GC_SDK_OFF_SI_STATUS_BASE = 0x3C0,        // 4 * u32 (chan 0..3)
  GC_SDK_OFF_SI_RESP_WORDS_BASE = 0x3D0,    // 4 * 2 * u32 (chan-major, word0/word1)

  // GX indirect texturing (GXBump.c)
  GC_SDK_OFF_GX_IREF = 0x400,              // u32
  GC_SDK_OFF_GX_IND_TEX_SCALE0 = 0x404,    // u32
  GC_SDK_OFF_GX_IND_TEX_SCALE1 = 0x408,    // u32
  GC_SDK_OFF_GX_TEV_IND_BASE = 0x410,      // 16 * u32 (stages 0..15)
  GC_SDK_OFF_GX_IND_MTX_BASE = 0x450,      // 9 * u32 (3 matrices * 3 regs each)
  GC_SDK_OFF_GX_GEN_MODE = 0x47C,          // u32 (genMode mirror for trace tests)

  // GX TEV konstant (GXTev.c)
  GC_SDK_OFF_GX_TEV_KSEL_BASE = 0x480,    // 8 * u32 (ksel[0..7])
  GC_SDK_OFF_GX_TEV_KCOLOR_RA_BASE = 0x4A0, // 4 * u32 (kcolor RA for id 0..3)
  GC_SDK_OFF_GX_TEV_KCOLOR_BG_BASE = 0x4B0, // 4 * u32 (kcolor BG for id 0..3)
  GC_SDK_OFF_GX_TEV_COLORS10_RA_LAST = 0x4C0, // u32 (last S10 RA reg)
  GC_SDK_OFF_GX_TEV_COLORS10_BG_LAST = 0x4C4, // u32 (last S10 BG reg)
};
