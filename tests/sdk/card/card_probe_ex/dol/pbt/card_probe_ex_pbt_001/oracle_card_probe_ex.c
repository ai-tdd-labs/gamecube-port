#include <stdint.h>

typedef uint32_t u32;
typedef int32_t s32;
typedef uint8_t u8;

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_WRONGDEVICE = -2,
  CARD_RESULT_NOCARD = -3,
  CARD_RESULT_FATAL_ERROR = -128,
};

// EXI stub state (DOL oracle config).
s32 gc_exi_probeex_ret[3] = { -1, -1, -1 };
u32 gc_exi_getid_ok[3];
u32 gc_exi_id[3];
u32 gc_exi_state[3];

enum { EXI_STATE_ATTACHED = 0x08 };

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

typedef struct {
  s32 attached;
  s32 mount_step;
  s32 size_mb;
  s32 sector_size;
} GcCardControl;

GcCardControl gc_card_block[2];

static uint16_t s_card_vendor_id = 0xFFFFu;

static int IsCard(u32 id) {
  u32 size;
  s32 sectorSize;
  if ((id & 0xFFFF0000u) && (id != 0x80000004u || s_card_vendor_id == 0xFFFFu)) return 0;
  if ((id & 3u) != 0) return 0;
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
  if (sectorSize == 0) return 0;
  if ((size * 1024u * 1024u / 8u) / (u32)sectorSize < 8u) return 0;
  return 1;
}

// Minimal EXI stubs with deterministic knobs.
s32 EXIProbeEx(s32 channel) {
  if (channel < 0 || channel >= 3) return -1;
  return gc_exi_probeex_ret[channel];
}
u32 EXIGetState(s32 channel) {
  if (channel < 0 || channel >= 3) return 0;
  return gc_exi_state[channel];
}
s32 EXIGetID(s32 channel, u32 device, u32* id) {
  (void)device;
  if (channel < 0 || channel >= 3) return 0;
  if (!gc_exi_getid_ok[channel]) { if (id) *id = 0; return 0; }
  if (id) *id = gc_exi_id[channel];
  return 1;
}

s32 oracle_CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize) {
  u32 id = 0;
  GcCardControl* card;
  s32 result;
  int probe;

  if (chan < 0 || 2 <= chan) return CARD_RESULT_FATAL_ERROR;

  // GameChoice at 0x800030E3 (low memory). In oracle, treat it as plain RAM.
  volatile u8* gc = (volatile u8*)0x800030E3u;
  if ((*gc) & 0x80u) return CARD_RESULT_NOCARD;

  card = &gc_card_block[chan];
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
  return result;
}

