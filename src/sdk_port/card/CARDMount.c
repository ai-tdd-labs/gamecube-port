#include <stdint.h>

#include "gc_mem.h"

#include "card_bios.h"

#include "dolphin/exi.h"

typedef uint32_t u32;
typedef int32_t s32;
typedef uint8_t u8;

// CARD result codes (evidence: MP4 disasm @ 0x800D4840).
enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_WRONGDEVICE = -2,
  CARD_RESULT_NOCARD = -3,
  CARD_RESULT_FATAL_ERROR = -128,
};

// SDK low-memory byte at 0x800030E3 (decomp: decomp_mario_party_4/src/dolphin/card/CARDMount.c).
static inline u8* gamechoice_ptr(void) { return (u8*)gc_mem_ptr(0x800030E3u, 1); }

static inline int OSDisableInterrupts(void) { return 1; }
static inline void OSRestoreInterrupts(int enabled) { (void)enabled; }

static u32 SectorSizeTable[8] = {
    8 * 1024,
    16 * 1024,
    32 * 1024,
    64 * 1024,
    128 * 1024,
    256 * 1024,
    0,
    0,
};

static uint16_t s_card_vendor_id = 0xFFFFu;

static int IsCard(u32 id) {
  u32 size;
  s32 sectorSize;

  if ((id & 0xFFFF0000u) && (id != 0x80000004u || s_card_vendor_id == 0xFFFFu)) {
    return 0;
  }
  if ((id & 3u) != 0) {
    return 0;
  }

  size = id & 0xFCu;
  switch (size) {
    case 4:
    case 8:
    case 16:
    case 32:
    case 64:
    case 128:
      break;
    default:
      return 0;
  }

  sectorSize = (s32)SectorSizeTable[(id & 0x00003800u) >> 11];
  if (sectorSize == 0) {
    return 0;
  }

  if ((size * 1024u * 1024u / 8u) / (u32)sectorSize < 8u) {
    return 0;
  }

  return 1;
}

s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize) {
  u32 id = 0;
  GcCardControl* card;
  int enabled;
  s32 result;
  int probe;

  if (chan < 0 || 2 <= chan) {
    return CARD_RESULT_FATAL_ERROR;
  }

  u8* gc = gamechoice_ptr();
  if (gc && ((*gc) & 0x80u)) {
    return CARD_RESULT_NOCARD;
  }

  card = &gc_card_block[chan];
  enabled = OSDisableInterrupts();

  probe = EXIProbeEx(chan);
  if (probe == -1) {
    result = CARD_RESULT_NOCARD;
  } else if (probe == 0) {
    result = CARD_RESULT_BUSY;
  } else if (card->attached) {
    if (card->mount_step < 1) {
      result = CARD_RESULT_BUSY;
    } else {
      if (memSize) *memSize = card->size_mb;
      if (sectorSize) *sectorSize = card->sector_size;
      result = CARD_RESULT_READY;
    }
  } else if ((EXIGetState(chan) & EXI_STATE_ATTACHED) != 0) {
    result = CARD_RESULT_WRONGDEVICE;
  } else if (!EXIGetID(chan, 0, &id)) {
    result = CARD_RESULT_BUSY;
  } else if (IsCard(id)) {
    if (memSize) *memSize = (s32)(id & 0xFCu);
    if (sectorSize) *sectorSize = (s32)SectorSizeTable[(id & 0x00003800u) >> 11];
    result = CARD_RESULT_READY;
  } else {
    result = CARD_RESULT_WRONGDEVICE;
  }

  OSRestoreInterrupts(enabled);
  return result;
}
