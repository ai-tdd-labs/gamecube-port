#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef int32_t s32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

// Results (decomp dolphin/card.h)
enum {
  CARD_RESULT_UNLOCKED = 1,
  CARD_RESULT_READY = 0,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_WRONGDEVICE = -2,
  CARD_RESULT_NOCARD = -3,
  CARD_RESULT_IOERROR = -5,
  CARD_RESULT_FATAL_ERROR = -128,
};

typedef void (*CARDCallback)(s32 chan, s32 result);
typedef void (*EXICallback)(s32 chan, void* context);

enum { EXI_STATE_ATTACHED = 0x08, EXI_STATE_LOCKED = 0x10 };
enum { EXI_READ = 0, EXI_WRITE = 1 };

// ---------- Minimal EXI oracle with status register ----------

s32 oracle_exi_probeex_ret[3] = { -1, -1, -1 };
u32 oracle_exi_getid_ok[3];
u32 oracle_exi_id[3];
u32 oracle_exi_attach_ok[3] = { 1, 1, 1 };

u32 oracle_exi_state[3];

u32 oracle_exi_card_status[3];
u32 oracle_exi_card_status_reads[3];
u32 oracle_exi_card_status_clears[3];

static u32 pending_status_read[3];

static EXICallback oracle_exi_exi_cb[3];

static EXICallback EXISetExiCallback(s32 channel, EXICallback cb) {
  if (channel < 0 || channel >= 3) return 0;
  EXICallback prev = oracle_exi_exi_cb[channel];
  oracle_exi_exi_cb[channel] = cb;
  return prev;
}

static u32 EXIGetState(s32 channel) {
  if (channel < 0 || channel >= 3) return 0;
  return oracle_exi_state[channel];
}

static BOOL EXIProbe(s32 channel) {
  if (channel < 0 || channel >= 3) return FALSE;
  return oracle_exi_probeex_ret[channel] > 0;
}

static s32 EXIProbeEx(s32 channel) {
  if (channel < 0 || channel >= 3) return -1;
  return oracle_exi_probeex_ret[channel];
}

static BOOL EXIGetID(s32 channel, u32 device, u32* id) {
  (void)device;
  if (channel < 0 || channel >= 3) return FALSE;
  if (!oracle_exi_getid_ok[channel]) {
    if (id) *id = 0;
    return FALSE;
  }
  if (id) *id = oracle_exi_id[channel];
  return TRUE;
}

static BOOL EXIAttach(s32 channel, EXICallback cb) {
  (void)cb;
  if (channel < 0 || channel >= 3) return FALSE;
  if (!oracle_exi_attach_ok[channel]) return FALSE;
  oracle_exi_state[channel] |= EXI_STATE_ATTACHED;
  return TRUE;
}

static BOOL EXILock(s32 channel, u32 device, EXICallback cb) {
  (void)device;
  (void)cb;
  if (channel < 0 || channel >= 3) return FALSE;
  if (oracle_exi_state[channel] & EXI_STATE_LOCKED) return FALSE;
  oracle_exi_state[channel] |= EXI_STATE_LOCKED;
  return TRUE;
}

static BOOL EXIUnlock(s32 channel) {
  if (channel < 0 || channel >= 3) return FALSE;
  oracle_exi_state[channel] &= ~EXI_STATE_LOCKED;
  return TRUE;
}

static BOOL EXISelect(s32 channel, u32 device, u32 frequency) {
  (void)device;
  (void)frequency;
  if (channel < 0 || channel >= 3) return FALSE;
  if ((oracle_exi_state[channel] & EXI_STATE_LOCKED) == 0) return FALSE;
  return TRUE;
}

static BOOL EXIDeselect(s32 channel) {
  if (channel < 0 || channel >= 3) return FALSE;
  return TRUE;
}

static BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, EXICallback cb) {
  (void)cb;
  if (channel < 0 || channel >= 3) return FALSE;
  if (!buffer || length <= 0 || length > 4) return FALSE;
  u8* b = (u8*)buffer;

  if (type == EXI_WRITE) {
    if (b[0] == 0x83u) {
      pending_status_read[channel] = 1;
    } else if (b[0] == 0x89u) {
      pending_status_read[channel] = 0;
      oracle_exi_card_status[channel] &= ~0x18u;
      oracle_exi_card_status_clears[channel]++;
    }
    return TRUE;
  }

  if (type == EXI_READ) {
    if (length == 1 && pending_status_read[channel]) {
      pending_status_read[channel] = 0;
      oracle_exi_card_status_reads[channel]++;
      b[0] = (u8)(oracle_exi_card_status[channel] & 0xFFu);
      return TRUE;
    }
    b[0] = 0;
    return TRUE;
  }

  return FALSE;
}

static BOOL EXISync(s32 channel) {
  if (channel < 0 || channel >= 3) return FALSE;
  return TRUE;
}

// ---------- SRAM oracle (only OSSramEx flash fields needed) ----------

typedef struct OSSramEx {
  u8 flashID[2][12];
  u32 wirelessKeyboardID;
  u16 wirelessPadID[4];
  u8 dvdErrorCode;
  u8 _padding0;
  u8 flashIDCheckSum[2];
  u16 gbs;
  u8 _padding1[2];
} OSSramEx;

static OSSramEx s_sram_ex;
static int s_sram_locked;

static OSSramEx* __OSLockSramEx(void) {
  if (s_sram_locked) return 0;
  s_sram_locked = 1;
  return &s_sram_ex;
}

static BOOL __OSUnlockSramEx(BOOL commit) {
  (void)commit;
  s_sram_locked = 0;
  return TRUE;
}

// ---------- CARD oracle (DoMount step0 + MountAsync wrapper) ----------

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

static u32 LatencyTable[8] = { 4, 8, 16, 32, 64, 128, 256, 512 };

static u16 s_card_vendor_id = 0xFFFFu;

static int IsCard(u32 id) {
  u32 size;
  u32 sectorSize;
  if ((id & 0xFFFF0000u) && (id != 0x80000004u || s_card_vendor_id == 0xFFFFu)) return 0;
  if ((id & 3u) != 0) return 0;
  size = id & 0xFCu;
  switch (size) {
    case 4: case 8: case 16: case 32: case 64: case 128:
      break;
    default:
      return 0;
  }
  sectorSize = SectorSizeTable[(id & 0x00003800u) >> 11];
  if (sectorSize == 0) return 0;
  if ((size * 1024u * 1024u / 8u) / sectorSize < 8u) return 0;
  return 1;
}

typedef struct {
  s32 result;
  s32 attached;
  s32 mount_step;
  s32 size_mb;
  s32 sector_size;
  u32 cid;
  u32 cblock;
  u32 latency;
  u8 id[12];
} GcCardControl;

GcCardControl gc_card_block[2];

// Status ops implemented via EXI.
static s32 __CARDReadStatus(s32 chan, u8* status) {
  BOOL err;
  u8 cmd[2] = { 0x83, 0x00 };
  if (!EXISelect(chan, 0, 4)) return CARD_RESULT_NOCARD;
  err = FALSE;
  err |= !EXIImm(chan, cmd, 2, EXI_WRITE, 0);
  err |= !EXISync(chan);
  err |= !EXIImm(chan, status, 1, EXI_READ, 0);
  err |= !EXISync(chan);
  err |= !EXIDeselect(chan);
  return err ? CARD_RESULT_NOCARD : CARD_RESULT_READY;
}

static s32 __CARDClearStatus(s32 chan) {
  BOOL err;
  u8 cmd = 0x89;
  if (!EXISelect(chan, 0, 4)) return CARD_RESULT_NOCARD;
  err = FALSE;
  err |= !EXIImm(chan, &cmd, 1, EXI_WRITE, 0);
  err |= !EXISync(chan);
  err |= !EXIDeselect(chan);
  return err ? CARD_RESULT_NOCARD : CARD_RESULT_READY;
}

// Deterministic unlock stub: test provides bytes.
int oracle_card_unlock_ok[2] = { 1, 1 };
u8 oracle_card_unlock_flash_id[2][12];
int oracle_card_unlock_calls[2];

static s32 __CARDUnlock(s32 chan, u8 flashID[12]) {
  if (chan < 0 || chan >= 2) return CARD_RESULT_NOCARD;
  oracle_card_unlock_calls[chan]++;
  if (!oracle_card_unlock_ok[chan]) return CARD_RESULT_NOCARD;
  memcpy(flashID, oracle_card_unlock_flash_id[chan], 12);
  return CARD_RESULT_READY;
}

static s32 DoMount_step0(s32 chan) {
  GcCardControl* card = &gc_card_block[chan];
  u32 id;
  u8 status;
  s32 result;
  OSSramEx* sram;
  u8 checkSum;

  if (card->mount_step != 0) return CARD_RESULT_READY;

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
    for (int i = 0; i < 12; i++) {
      sram->flashID[chan][i] = card->id[i];
      checkSum = (u8)(checkSum + card->id[i]);
    }
    sram->flashIDCheckSum[chan] = (u8)~checkSum;
    (void)__OSUnlockSramEx(TRUE);

    return result;
  }

  card->mount_step = 1;

  checkSum = 0;
  sram = __OSLockSramEx();
  for (int i = 0; i < 12; i++) {
    checkSum = (u8)(checkSum + sram->flashID[chan][i]);
  }
  (void)__OSUnlockSramEx(FALSE);
  if (sram->flashIDCheckSum[chan] != (u8)~checkSum) {
    result = CARD_RESULT_IOERROR;
    goto error;
  }

  return CARD_RESULT_READY;

error:
  (void)EXIUnlock(chan);
  card->result = result;
  card->attached = 0;
  return result;
}

s32 oracle_CARDMountAsync_step0(s32 chan) {
  GcCardControl* card = &gc_card_block[chan];
  card->result = CARD_RESULT_BUSY;
  card->mount_step = 0;
  card->attached = 1;

  (void)EXILock(chan, 0, 0);
  return DoMount_step0(chan);
}

void oracle_reset(void) {
  memset(gc_card_block, 0, sizeof(gc_card_block));
  memset(&s_sram_ex, 0, sizeof(s_sram_ex));
  s_sram_locked = 0;

  memset(oracle_exi_state, 0, sizeof(oracle_exi_state));
  memset(oracle_exi_getid_ok, 0, sizeof(oracle_exi_getid_ok));
  memset(oracle_exi_id, 0, sizeof(oracle_exi_id));
  memset(oracle_exi_card_status, 0, sizeof(oracle_exi_card_status));
  memset(oracle_exi_card_status_reads, 0, sizeof(oracle_exi_card_status_reads));
  memset(oracle_exi_card_status_clears, 0, sizeof(oracle_exi_card_status_clears));
  memset(pending_status_read, 0, sizeof(pending_status_read));

  memset(oracle_card_unlock_ok, 0, sizeof(oracle_card_unlock_ok));
  memset(oracle_card_unlock_flash_id, 0, sizeof(oracle_card_unlock_flash_id));
  memset(oracle_card_unlock_calls, 0, sizeof(oracle_card_unlock_calls));
}

void oracle_set_sram_flashid(int chan, const u8* id12) {
  OSSramEx* s = &s_sram_ex;
  u8 c = 0;
  for (int i = 0; i < 12; i++) {
    s->flashID[chan][i] = id12[i];
    c = (u8)(c + id12[i]);
  }
  s->flashIDCheckSum[chan] = (u8)~c;
}

void oracle_get_sram_flashid(int chan, u8* out12, u8* outchk) {
  OSSramEx* s = &s_sram_ex;
  memcpy(out12, s->flashID[chan], 12);
  *outchk = s->flashIDCheckSum[chan];
}

void oracle_corrupt_sram_checksum(int chan, u8 xor_mask) {
  s_sram_ex.flashIDCheckSum[chan] ^= xor_mask;
}
