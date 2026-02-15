#include <stdint.h>

#include "gc_mem.h"

#include "card_bios.h"

#include "dolphin/exi.h"
#include "dolphin/OSRtcPriv.h"

typedef uint32_t u32;
typedef int32_t s32;
typedef uint8_t u8;

// CARD result codes (evidence: MP4 disasm @ 0x800D4840).
enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_WRONGDEVICE = -2,
  CARD_RESULT_NOCARD = -3,
  CARD_RESULT_UNLOCKED = -4,
  CARD_RESULT_IOERROR = -5,
  CARD_RESULT_FATAL_ERROR = -128,
};

typedef void (*CARDCallback)(s32 chan, s32 result);

// SDK low-memory byte at 0x800030E3 (decomp: decomp_mario_party_4/src/dolphin/card/CARDMount.c).
static inline u8* gamechoice_ptr(void) { return (u8*)gc_mem_ptr(0x800030E3u, 1); }

static inline int OSDisableInterrupts(void) { return 1; }
static inline void OSRestoreInterrupts(int enabled) { (void)enabled; }

static void OSCancelAlarm(uint32_t* calls) {
  if (calls) (*calls)++;
}

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

static u32 LatencyTable[8] = {
    4,
    8,
    16,
    32,
    64,
    128,
    256,
    512,
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

// From sdk_port/card/card_bios.c
s32 __CARDClearStatus(s32 chan);
s32 __CARDReadStatus(s32 chan, u8* status);
s32 __CARDEnableInterrupt(s32 chan, int enable);
s32 __CARDRead(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback);
s32 __CARDPutControlBlock(GcCardControl* card, s32 result);
s32 __CARDGetControlBlock(s32 chan, GcCardControl** pcard);

// From sdk_port/card/CARDCheck.c
s32 __CARDVerify(GcCardControl* card);

// Deterministic stub (for now): implemented in sdk_port/card/card_unlock.c.
s32 __CARDUnlock(s32 chan, u8 flashID[12]);

// From sdk_port/os/OSRtc.c (host model).
OSSramEx* __OSLockSramEx(void);
BOOL __OSUnlockSramEx(BOOL commit);

static s32 DoMount(s32 chan);
static s32 DoMount_step0(s32 chan);

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

enum { CARD_SYSTEM_BLOCK_SIZE = 8 * 1024 };
enum { CARD_WORKAREA_SIZE = 5 * CARD_SYSTEM_BLOCK_SIZE };
enum { CARD_MAX_MOUNT_STEP = 7 };

static void __CARDExtHandler(s32 chan, void* context) {
  (void)chan;
  (void)context;
}

static void __CARDExiHandler(s32 chan, void* context) {
  (void)chan;
  (void)context;
}

static void __CARDUnlockedHandler(s32 chan, void* context) {
  (void)chan;
  (void)context;
}

static void DoUnmount(s32 chan, s32 result) {
  GcCardControl* card = &gc_card_block[chan];
  int enabled = OSDisableInterrupts();
  if (card->attached) {
    EXISetExiCallback(chan, 0);
    EXIDetach(chan);
    OSCancelAlarm(&card->alarm_cancel_calls);
    card->attached = 0;
    card->result = result;
    card->mount_step = 0;
  }
  OSRestoreInterrupts(enabled);
}

static void __CARDMountCallback(s32 chan, s32 result) {
  GcCardControl* card = &gc_card_block[chan];
  CARDCallback callback;

  switch (result) {
    case CARD_RESULT_READY:
      if (++card->mount_step < CARD_MAX_MOUNT_STEP) {
        result = DoMount(chan);
        if (0 <= result) {
          return;
        }
      } else {
        result = __CARDVerify(card);
      }
      break;
    case CARD_RESULT_UNLOCKED:
      card->unlock_callback = (uintptr_t)__CARDMountCallback;
      if (!EXILock(chan, 0, (EXICallback)__CARDUnlockedHandler)) {
        return;
      }
      card->unlock_callback = 0;

      result = DoMount(chan);
      if (0 <= result) {
        return;
      }
      break;
    case CARD_RESULT_IOERROR:
    case CARD_RESULT_NOCARD:
      DoUnmount(chan, result);
      break;
    default:
      break;
  }

  callback = (CARDCallback)(uintptr_t)card->api_callback;
  card->api_callback = 0;
  __CARDPutControlBlock(card, result);
  if (callback) {
    callback(chan, result);
  }
}

static s32 DoMount(s32 chan) {
  GcCardControl* card = &gc_card_block[chan];
  s32 result;

  if (card->mount_step == 0) {
    result = DoMount_step0(chan);
    if (result < 0 || card->mount_step == 0) {
      return result;
    }
  }

  if (card->mount_step == 1) {
    OSSramEx* sram;
    uint16_t vendorID = 0;
    if (card->cid == 0x80000004u) {
      sram = __OSLockSramEx();
      vendorID =
          (uint16_t)(((uint16_t)sram->flashID[chan][0] << 8) |
                     (uint16_t)sram->flashID[chan][1]);
      (void)__OSUnlockSramEx(0);

      if (s_card_vendor_id == 0xFFFFu || vendorID != s_card_vendor_id) {
        return CARD_RESULT_WRONGDEVICE;
      }
    }

    card->mount_step = 2;
    result = __CARDEnableInterrupt(chan, 1);
    if (result < 0) {
      return result;
    }

    EXISetExiCallback(chan, (EXICallback)__CARDExiHandler);
    EXIUnlock(chan);
    // Cache modeling is out-of-scope for the host port; the mount workflow
    // correctness is validated via expected.bin vs actual.bin dumps.
  }

  result = __CARDRead(
      chan,
      (u32)card->sector_size * (uint32_t)(card->mount_step - 2),
      (s32)CARD_SYSTEM_BLOCK_SIZE,
      (u8*)(uintptr_t)card->work_area +
          (uintptr_t)CARD_SYSTEM_BLOCK_SIZE * (uintptr_t)(card->mount_step - 2),
      __CARDMountCallback);
  if (result < 0) {
    __CARDPutControlBlock(card, result);
  }
  return result;
}

static s32 DoMount_step0(s32 chan) {
  GcCardControl* card = &gc_card_block[chan];
  u32 id = 0;
  u8 status = 0;
  s32 result;
  OSSramEx* sram;
  int i;
  u8 checkSum;

  if (!EXIGetID(chan, 0, &id)) {
    result = CARD_RESULT_NOCARD;
  } else if (IsCard(id)) {
    result = CARD_RESULT_READY;
  } else {
    result = CARD_RESULT_WRONGDEVICE;
  }
  if (result < 0) goto error;

  card->cid = id;
  card->size_mb = (s32)(id & 0xFCu);
  card->size_u16 = (uint16_t)(id & 0xFCu);
  card->sector_size = (s32)SectorSizeTable[(id & 0x00003800u) >> 11];
  card->cblock = (u32)((card->size_mb * 1024u * 1024u / 8u) / (u32)card->sector_size);
  card->latency = LatencyTable[(id & 0x00000700u) >> 8];

  result = __CARDClearStatus(chan);
  if (result < 0) goto error;
  result = __CARDReadStatus(chan, &status);
  if (result < 0) goto error;

  if (!EXIProbe(chan)) {
    result = CARD_RESULT_NOCARD;
    goto error;
  }

  if ((status & 0x40u) == 0) {
    result = __CARDUnlock(chan, card->id);
    if (result < 0) goto error;

    checkSum = 0;
    sram = __OSLockSramEx();
    for (i = 0; i < 12; i++) {
      sram->flashID[chan][i] = card->id[i];
      checkSum = (u8)(checkSum + card->id[i]);
    }
    sram->flashIDCheckSum[chan] = (u8)~checkSum;
    (void)__OSUnlockSramEx(1);

    return result;
  }

  card->mount_step = 1;
  checkSum = 0;
  sram = __OSLockSramEx();
  for (i = 0; i < 12; i++) {
    checkSum = (u8)(checkSum + sram->flashID[chan][i]);
  }
  (void)__OSUnlockSramEx(0);
  if (sram->flashIDCheckSum[chan] != (u8)~checkSum) {
    result = CARD_RESULT_IOERROR;
    goto error;
  }

  return CARD_RESULT_READY;

error:
  EXIUnlock(chan);
  if (card->attached) {
    card->result = result;
    card->attached = 0;
  }
  return result;
}

s32 CARDMountAsync_step0(s32 chan, void* workArea, CARDCallback detachCallback,
                        CARDCallback attachCallback) {
  (void)workArea;
  (void)detachCallback;
  (void)attachCallback;

  GcCardControl* card;
  u8* gc;

  if (chan < 0 || 2 <= chan) {
    return CARD_RESULT_FATAL_ERROR;
  }

  gc = gamechoice_ptr();
  if (gc && ((*gc) & 0x80u)) {
    return CARD_RESULT_NOCARD;
  }

  card = &gc_card_block[chan];
  card->result = CARD_RESULT_BUSY;
  card->mount_step = 0;
  card->attached = 1;
  (void)EXILock(chan, 0, 0);
  return DoMount_step0(chan);
}

s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback) {
  GcCardControl* card;
  int enabled;
  u8* gc;

  if (chan < 0 || 2 <= chan) {
    return CARD_RESULT_FATAL_ERROR;
  }

  gc = gamechoice_ptr();
  if (gc && ((*gc) & 0x80u)) {
    return CARD_RESULT_NOCARD;
  }

  card = &gc_card_block[chan];

  enabled = OSDisableInterrupts();
  if (card->result == CARD_RESULT_BUSY) {
    OSRestoreInterrupts(enabled);
    return CARD_RESULT_BUSY;
  }

  if (!card->attached && (EXIGetState(chan) & EXI_STATE_ATTACHED)) {
    OSRestoreInterrupts(enabled);
    return CARD_RESULT_WRONGDEVICE;
  }

  card->result = CARD_RESULT_BUSY;
  card->work_area = (uintptr_t)workArea;
  card->ext_callback = (uintptr_t)detachCallback;
  card->api_callback = (uintptr_t)(attachCallback ? attachCallback : __CARDDefaultApiCallback);
  card->exi_callback = 0;

  if (!card->attached && !EXIAttach(chan, (EXICallback)__CARDExtHandler)) {
    card->result = CARD_RESULT_NOCARD;
    OSRestoreInterrupts(enabled);
    return CARD_RESULT_NOCARD;
  }

  card->mount_step = 0;
  card->attached = 1;
  EXISetExiCallback(chan, 0);
  OSCancelAlarm(&card->alarm_cancel_calls);
  card->current_dir = 0;
  card->current_fat = 0;

  OSRestoreInterrupts(enabled);

  card->unlock_callback = (uintptr_t)__CARDMountCallback;
  if (!EXILock(chan, 0, (EXICallback)__CARDUnlockedHandler)) {
    return CARD_RESULT_READY;
  }
  card->unlock_callback = 0;

  return DoMount(chan);
}

s32 CARDMount(s32 chan, void* workArea, CARDCallback attachCb) {
  s32 result = CARDMountAsync(chan, workArea, attachCb, __CARDDefaultApiCallback);
  if (result < 0) {
    return result;
  }

  return __CARDSync(chan);
}

s32 CARDUnmount(s32 chan) {
  GcCardControl* card;
  s32 result = __CARDGetControlBlock(chan, &card);
  if (result < 0) {
    return result;
  }
  DoUnmount(chan, CARD_RESULT_NOCARD);
  return CARD_RESULT_READY;
}
