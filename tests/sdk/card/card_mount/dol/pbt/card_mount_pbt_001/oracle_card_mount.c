#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
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
  CARD_RESULT_BROKEN = -6,
  CARD_RESULT_ENCODING = -13,
  CARD_RESULT_FATAL_ERROR = -128,
};

typedef void (*CARDCallback)(s32 chan, s32 result);
typedef void (*EXICallback)(s32 chan, void* context);
typedef struct OSThreadQueue { void* head; void* tail; } OSThreadQueue;

static int OSDisableInterrupts(void) { return 1; }
static void OSRestoreInterrupts(int level) { (void)level; }
static void OSSleepThread(OSThreadQueue* queue) { (void)queue; }
static void OSWakeupThread(OSThreadQueue* queue) { (void)queue; }

enum { EXI_STATE_ATTACHED = 0x08, EXI_STATE_LOCKED = 0x10 };

// --- Minimal SRAMEx oracle (flashID + checksum) ---
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
static BOOL __OSUnlockSramEx(BOOL commit) { (void)commit; s_sram_locked = 0; return TRUE; }

static u16 s_font_encode;
static u16 OSGetFontEncode(void) { return s_font_encode; }

// --- Minimal EXI oracle knobs ---
s32 oracle_exi_probeex_ret[3] = { -1, -1, -1 };
u32 oracle_exi_getid_ok[3];
u32 oracle_exi_id[3];
u32 oracle_exi_attach_ok[3] = { 1, 1, 1 };
u32 oracle_exi_state[3];
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
static BOOL EXIProbe(s32 channel) { return (channel >= 0 && channel < 3) ? (oracle_exi_probeex_ret[channel] > 0) : FALSE; }
static BOOL EXIGetID(s32 channel, u32 device, u32* id) {
  (void)device;
  if (channel < 0 || channel >= 3) return FALSE;
  if (!oracle_exi_getid_ok[channel]) { if (id) *id = 0; return FALSE; }
  if (id) *id = oracle_exi_id[channel];
  return TRUE;
}

// --- CARD status register oracle (just enough for DoMount step0) ---
u32 oracle_exi_card_status[3];
u32 oracle_exi_card_status_reads[3];
u32 oracle_exi_card_status_clears[3];

static s32 __CARDClearStatus(s32 chan) {
  if (chan < 0 || chan >= 3) return CARD_RESULT_NOCARD;
  oracle_exi_card_status[chan] &= ~0x18u;
  oracle_exi_card_status_clears[chan]++;
  return CARD_RESULT_READY;
}
static s32 __CARDReadStatus(s32 chan, u8* status) {
  if (chan < 0 || chan >= 3) return CARD_RESULT_NOCARD;
  oracle_exi_card_status_reads[chan]++;
  if (status) *status = (u8)(oracle_exi_card_status[chan] & 0xFFu);
  return CARD_RESULT_READY;
}

// --- Deterministic unlock stub ---
int oracle_card_unlock_ok[2] = { 1, 1 };
u8 oracle_card_unlock_flash_id[2][12];
int oracle_card_unlock_calls[2];

static s32 __CARDUnlock(s32 chan, u8 flashID[12]) {
  if (chan < 0 || chan >= 2) return CARD_RESULT_FATAL_ERROR;
  oracle_card_unlock_calls[chan]++;
  if (!oracle_card_unlock_ok[chan]) return CARD_RESULT_NOCARD;
  if (flashID) memcpy(flashID, oracle_card_unlock_flash_id[chan], 12);
  return CARD_RESULT_UNLOCKED;
}

// --- Minimal CARD verify (MP4 logic, subset) ---
enum { CARD_SYSTEM_BLOCK_SIZE = 8 * 1024, CARD_WORKAREA_SIZE = 5 * 8 * 1024 };
enum { CARD_MAX_FILE = 127, CARD_DIR_SIZE = 64 };
enum { CARD_NUM_SYSTEM_BLOCK = 5 };
enum { CARD_FAT_AVAIL = 0x0000u, CARD_FAT_CHECKSUM = 0, CARD_FAT_CHECKSUMINV = 1, CARD_FAT_CHECKCODE = 2, CARD_FAT_FREEBLOCKS = 3 };

static inline u16 be16(const u8* p) { return (u16)(((u16)p[0] << 8) | (u16)p[1]); }
static inline u64 be64(const u8* p) {
  u64 v = 0;
  for (int i = 0; i < 8; i++) v = (v << 8) | (u64)p[i];
  return v;
}
static void __CARDCheckSum_u16be(const u8* ptr, u32 length, u16* checksum, u16* checksumInv) {
  u32 n = length / 2u;
  u16 cs = 0, csi = 0;
  for (u32 i = 0; i < n; i++) {
    u16 v = be16(ptr + i * 2u);
    cs = (u16)(cs + v);
    csi = (u16)(csi + (u16)~v);
  }
  if (cs == 0xFFFFu) cs = 0;
  if (csi == 0xFFFFu) csi = 0;
  *checksum = cs;
  *checksumInv = csi;
}

typedef struct {
  BOOL attached;
  s32 result;
  u16 size;
  s32 sectorSize;
  u16 cBlock;
  s32 latency;
  u8 id[12];
  int mountStep;
  void* workArea;
  void* currentDir;
  void* currentFat;
  CARDCallback extCallback;
  CARDCallback apiCallback;
  CARDCallback exiCallback;
  CARDCallback unlockCallback;
  u32 cid;
  u32 thread_queue_inited;
} CARDControl;

CARDControl __CARDBlock[2];

static s32 CARDGetResultCode(s32 chan) {
  if (chan < 0 || 2 <= chan) {
    return CARD_RESULT_FATAL_ERROR;
  }
  return __CARDBlock[chan].result;
}

void __CARDSyncCallback(s32 chan, s32 result) {
  (void)result;
  OSWakeupThread((OSThreadQueue*)&__CARDBlock[chan].thread_queue_inited);
}

static s32 oracle___CARDSync(s32 chan) {
  s32 result;
  int enabled;
  CARDControl* block;

  if (chan < 0 || 2 <= chan) {
    return CARD_RESULT_FATAL_ERROR;
  }

  block = &__CARDBlock[chan];
  enabled = OSDisableInterrupts();
  while ((result = CARDGetResultCode(chan)) == CARD_RESULT_BUSY) {
    OSSleepThread((OSThreadQueue*)&block->thread_queue_inited);
  }
  OSRestoreInterrupts(enabled);
  return result;
}

static s32 __CARDPutControlBlock(CARDControl* card, s32 result) { card->result = result; return result; }

static s32 VerifyID(CARDControl* card) {
  const u8* id = (const u8*)card->workArea;
  if (be16(id + 32u) != 0) return CARD_RESULT_BROKEN;
  if (be16(id + 34u) != card->size) return CARD_RESULT_BROKEN;
  u16 cs=0,csi=0;
  __CARDCheckSum_u16be(id, 512u - 4u, &cs, &csi);
  if (be16(id + 508u) != cs || be16(id + 510u) != csi) return CARD_RESULT_BROKEN;
  if (be16(id + 36u) != OSGetFontEncode()) return CARD_RESULT_ENCODING;

  u64 rand = be64(id + 12u);
  OSSramEx* sramEx = __OSLockSramEx();
  if (!sramEx) return CARD_RESULT_BROKEN;
  int chan = (int)(card - __CARDBlock);
  for (int i = 0; i < 12; i++) {
    rand = (rand * 1103515245ull + 12345ull) >> 16;
    if (id[i] != (u8)(sramEx->flashID[chan][i] + (u8)rand)) {
      __OSUnlockSramEx(FALSE);
      return CARD_RESULT_BROKEN;
    }
    rand = ((rand * 1103515245ull + 12345ull) >> 16) & 0x7FFFull;
  }
  __OSUnlockSramEx(FALSE);
  return CARD_RESULT_READY;
}

static s32 VerifyDir(CARDControl* card) {
  u8* wa = (u8*)card->workArea;
  u8* dir[2];
  u8* chk[2];
  u16 cs=0,csi=0;
  int errors=0, current=0;
  for (int i=0;i<2;i++) {
    dir[i] = wa + (u32)(1+i)*CARD_SYSTEM_BLOCK_SIZE;
    chk[i] = dir[i] + (u32)CARD_MAX_FILE*CARD_DIR_SIZE;
    __CARDCheckSum_u16be(dir[i], (u32)CARD_SYSTEM_BLOCK_SIZE-4u, &cs, &csi);
    if (be16(chk[i]+(CARD_DIR_SIZE-4))!=cs || be16(chk[i]+(CARD_DIR_SIZE-2))!=csi) { errors++; current=i; card->currentDir=0; }
  }
  if (errors==0) {
    if (!card->currentDir) {
      int16_t cc0=(int16_t)be16(chk[0]+(CARD_DIR_SIZE-6));
      int16_t cc1=(int16_t)be16(chk[1]+(CARD_DIR_SIZE-6));
      current=((cc0-cc1)<0)?0:1;
      card->currentDir = (void*)dir[current];
      memcpy(dir[current], dir[current^1], CARD_SYSTEM_BLOCK_SIZE);
    } else {
      current=(card->currentDir==(void*)dir[0])?0:1;
    }
  }
  (void)current;
  return errors;
}

static s32 VerifyFAT(CARDControl* card) {
  u8* wa = (u8*)card->workArea;
  u8* fat[2];
  u16 cs=0,csi=0;
  int errors=0, current=0;
  for (int i=0;i<2;i++) {
    fat[i] = wa + (u32)(3+i)*CARD_SYSTEM_BLOCK_SIZE;
    __CARDCheckSum_u16be(fat[i] + (u32)CARD_FAT_CHECKCODE*2u, (u32)CARD_SYSTEM_BLOCK_SIZE-4u, &cs, &csi);
    if (be16(fat[i]+(u32)CARD_FAT_CHECKSUM*2u)!=cs || be16(fat[i]+(u32)CARD_FAT_CHECKSUMINV*2u)!=csi) { errors++; current=i; card->currentFat=0; continue; }
    u16 cFree=0;
    for (u16 b=CARD_NUM_SYSTEM_BLOCK; b<card->cBlock; b++) {
      u16 v = be16(fat[i] + (u32)b*2u);
      if (v==CARD_FAT_AVAIL) cFree++;
    }
    if (cFree != be16(fat[i] + (u32)CARD_FAT_FREEBLOCKS*2u)) { errors++; current=i; card->currentFat=0; continue; }
  }
  if (errors==0) {
    if (!card->currentFat) {
      int16_t cc0=(int16_t)be16(fat[0] + (u32)CARD_FAT_CHECKCODE*2u);
      int16_t cc1=(int16_t)be16(fat[1] + (u32)CARD_FAT_CHECKCODE*2u);
      current=((cc0-cc1)<0)?0:1;
      card->currentFat = (void*)fat[current];
      memcpy(fat[current], fat[current^1], CARD_SYSTEM_BLOCK_SIZE);
    } else {
      current=(card->currentFat==(void*)fat[0])?0:1;
    }
  }
  (void)current;
  return errors;
}

static s32 __CARDVerify(CARDControl* card) {
  s32 rc = VerifyID(card);
  if (rc < 0) return rc;
  int errors = VerifyDir(card) + VerifyFAT(card);
  return errors == 0 ? CARD_RESULT_READY : CARD_RESULT_BROKEN;
}

// --- System block backing store for oracle __CARDRead ---
static u8 s_card_img[0x40000];

static void oracle_seed_card_img_valid(void) {
  // Fill with deterministic noise, then we will overwrite system blocks 0..4 with valid layout.
  for (u32 i = 0; i < (u32)sizeof(s_card_img); i++) s_card_img[i] = (u8)(i ^ 0xA5u);
}

static int oracle_read_img(u32 addr, void* dst, u32 len) {
  if (!dst) return 0;
  if (addr + len > (u32)sizeof(s_card_img)) return 0;
  memcpy(dst, &s_card_img[addr], len);
  return 1;
}

static int oracle_write_img(u32 addr, const void* src, u32 len) {
  if (!src) return 0;
  if (addr + len > (u32)sizeof(s_card_img)) return 0;
  memcpy(&s_card_img[addr], src, len);
  return 1;
}

static void make_valid_system_blocks(int chan, u16 size_u16, u16 cblock, u16 encode) {
  // Work buffer in oracle space, then copied into image at offsets 0,0x2000,... for sectorSize==8KiB.
  u8 wa[CARD_WORKAREA_SIZE];
  memset(wa, 0, sizeof(wa));

  // ID block @0.
  // serial[0..11] should match flashID when rand seed==0.
  for (int i = 0; i < 12; i++) wa[i] = s_sram_ex.flashID[chan][i];
  // deviceID 0, size, encode.
  wa[32] = 0; wa[33] = 0;
  wa[34] = (u8)(size_u16 >> 8); wa[35] = (u8)size_u16;
  wa[36] = (u8)(encode >> 8); wa[37] = (u8)encode;
  u16 cs=0,csi=0;
  __CARDCheckSum_u16be(wa, 512u-4u, &cs, &csi);
  wa[508] = (u8)(cs >> 8); wa[509] = (u8)cs;
  wa[510] = (u8)(csi >> 8); wa[511] = (u8)csi;

  // DIR copies @0x2000 and @0x4000.
  for (u32 i = 0; i < CARD_SYSTEM_BLOCK_SIZE; i++) wa[0x2000u + i] = (u8)(0xA0u ^ (u8)i);
  for (u32 i = 0; i < CARD_SYSTEM_BLOCK_SIZE; i++) wa[0x4000u + i] = (u8)(0xB0u ^ (u8)i);
  // Set checkCode + checksums for both dirs.
  u8* dir0 = wa + 0x2000u;
  u8* dir1 = wa + 0x4000u;
  u8* chk0 = dir0 + (u32)CARD_MAX_FILE * CARD_DIR_SIZE;
  u8* chk1 = dir1 + (u32)CARD_MAX_FILE * CARD_DIR_SIZE;
  // checkCode at offset (64-6)
  chk0[64u-6u] = 0x00; chk0[64u-5u] = 0x01;
  chk1[64u-6u] = 0x00; chk1[64u-5u] = 0x02;
  __CARDCheckSum_u16be(dir0, (u32)CARD_SYSTEM_BLOCK_SIZE-4u, &cs, &csi);
  chk0[64u-4u] = (u8)(cs>>8); chk0[64u-3u] = (u8)cs;
  chk0[64u-2u] = (u8)(csi>>8); chk0[64u-1u] = (u8)csi;
  __CARDCheckSum_u16be(dir1, (u32)CARD_SYSTEM_BLOCK_SIZE-4u, &cs, &csi);
  chk1[64u-4u] = (u8)(cs>>8); chk1[64u-3u] = (u8)cs;
  chk1[64u-2u] = (u8)(csi>>8); chk1[64u-1u] = (u8)csi;

  // FAT copies @0x6000 and @0x8000 (big-endian u16 array stored in bytes).
  u8* fat0 = wa + 0x6000u;
  u8* fat1 = wa + 0x8000u;
  memset(fat0, 0, CARD_SYSTEM_BLOCK_SIZE);
  memset(fat1, 0, CARD_SYSTEM_BLOCK_SIZE);
  for (u16 b = CARD_NUM_SYSTEM_BLOCK; b < cblock; b++) {
    fat0[(u32)b*2u+0] = 0; fat0[(u32)b*2u+1] = 0;
    fat1[(u32)b*2u+0] = 0; fat1[(u32)b*2u+1] = 0;
  }
  // checkCode and freeblocks (and checksums) differ so selection is deterministic.
  fat0[(u32)CARD_FAT_CHECKCODE*2u+0] = 0; fat0[(u32)CARD_FAT_CHECKCODE*2u+1] = 1;
  fat1[(u32)CARD_FAT_CHECKCODE*2u+0] = 0; fat1[(u32)CARD_FAT_CHECKCODE*2u+1] = 2;
  u16 freeb = (u16)(cblock - CARD_NUM_SYSTEM_BLOCK);
  fat0[(u32)CARD_FAT_FREEBLOCKS*2u+0] = (u8)(freeb>>8); fat0[(u32)CARD_FAT_FREEBLOCKS*2u+1] = (u8)freeb;
  fat1[(u32)CARD_FAT_FREEBLOCKS*2u+0] = (u8)(freeb>>8); fat1[(u32)CARD_FAT_FREEBLOCKS*2u+1] = (u8)freeb;
  __CARDCheckSum_u16be(fat0 + (u32)CARD_FAT_CHECKCODE*2u, (u32)CARD_SYSTEM_BLOCK_SIZE-4u, &cs, &csi);
  fat0[(u32)CARD_FAT_CHECKSUM*2u+0] = (u8)(cs>>8); fat0[(u32)CARD_FAT_CHECKSUM*2u+1] = (u8)cs;
  fat0[(u32)CARD_FAT_CHECKSUMINV*2u+0] = (u8)(csi>>8); fat0[(u32)CARD_FAT_CHECKSUMINV*2u+1] = (u8)csi;
  __CARDCheckSum_u16be(fat1 + (u32)CARD_FAT_CHECKCODE*2u, (u32)CARD_SYSTEM_BLOCK_SIZE-4u, &cs, &csi);
  fat1[(u32)CARD_FAT_CHECKSUM*2u+0] = (u8)(cs>>8); fat1[(u32)CARD_FAT_CHECKSUM*2u+1] = (u8)cs;
  fat1[(u32)CARD_FAT_CHECKSUMINV*2u+0] = (u8)(csi>>8); fat1[(u32)CARD_FAT_CHECKSUMINV*2u+1] = (u8)csi;

  // Copy into the oracle image at offsets matching sectorSize=0x2000.
  oracle_write_img(0x0000u, wa + 0x0000u, CARD_SYSTEM_BLOCK_SIZE);
  oracle_write_img(0x2000u, wa + 0x2000u, CARD_SYSTEM_BLOCK_SIZE);
  oracle_write_img(0x4000u, wa + 0x4000u, CARD_SYSTEM_BLOCK_SIZE);
  oracle_write_img(0x6000u, wa + 0x6000u, CARD_SYSTEM_BLOCK_SIZE);
  oracle_write_img(0x8000u, wa + 0x8000u, CARD_SYSTEM_BLOCK_SIZE);
}

// --- Mount implementation (MP4 structure, but __CARDRead is a direct image copy) ---
static u32 SectorSizeTable[8] = { 8*1024,16*1024,32*1024,64*1024,128*1024,256*1024,0,0 };
static u32 LatencyTable[8] = { 4,8,16,32,64,128,256,512 };
static u16 __CARDVendorID = 0xFFFFu;

static int IsCard(u32 id) {
  u32 size = id & 0xFCu;
  if ((id & 0xFFFF0000u) && (id != 0x80000004u || __CARDVendorID == 0xFFFFu)) return 0;
  if ((id & 3u) != 0) return 0;
  switch (size) { case 4: case 8: case 16: case 32: case 64: case 128: break; default: return 0; }
  u32 ss = SectorSizeTable[(id & 0x00003800u) >> 11];
  if (ss == 0) return 0;
  if ((size * 1024u * 1024u / 8u) / ss < 8u) return 0;
  return 1;
}

static void __CARDDefaultApiCallback(s32 chan, s32 result) { (void)chan; (void)result; }
static void __CARDExtHandler(s32 chan, void* context) { (void)chan; (void)context; }
static void __CARDUnlockedHandler(s32 chan, void* context) { (void)chan; (void)context; }

static void DoUnmount(s32 chan, s32 result) {
  CARDControl* card = &__CARDBlock[chan];
  card->result = result;
  card->attached = FALSE;
}

static s32 __CARDEnableInterrupt(s32 chan, BOOL enable) { (void)chan; (void)enable; return CARD_RESULT_READY; }

static s32 __CARDRead(s32 chan, u32 addr, s32 length, void* dst, CARDCallback cb) {
  (void)chan;
  if (!oracle_read_img(addr, dst, (u32)length)) return CARD_RESULT_NOCARD;
  if (cb) cb(chan, CARD_RESULT_READY);
  return CARD_RESULT_READY;
}

static s32 DoMount(s32 chan);

void __CARDMountCallback(s32 chan, s32 result) {
  CARDControl* card = &__CARDBlock[chan];
  CARDCallback callback;

  switch (result) {
    case CARD_RESULT_READY:
      if (++card->mountStep < (CARD_NUM_SYSTEM_BLOCK + 2)) {
        result = DoMount(chan);
        if (0 <= result) return;
      } else {
        result = __CARDVerify(card);
      }
      break;
    case CARD_RESULT_UNLOCKED:
      card->unlockCallback = __CARDMountCallback;
      if (!EXILock(chan, 0, __CARDUnlockedHandler)) return;
      card->unlockCallback = 0;
      result = DoMount(chan);
      if (0 <= result) return;
      break;
    case CARD_RESULT_IOERROR:
    case CARD_RESULT_NOCARD:
      DoUnmount(chan, result);
      break;
  }

  callback = card->apiCallback ? card->apiCallback : __CARDDefaultApiCallback;
  card->apiCallback = 0;
  __CARDPutControlBlock(card, result);
  callback(chan, result);
}

static s32 DoMount(s32 chan) {
  CARDControl* card = &__CARDBlock[chan];
  u32 id = 0;
  u8 status = 0;
  s32 result;
  OSSramEx* sram;
  u8 checkSum;
  int step;

  if (card->mountStep == 0) {
    if (!EXIGetID(chan, 0, &id)) result = CARD_RESULT_NOCARD;
    else if (IsCard(id)) result = CARD_RESULT_READY;
    else result = CARD_RESULT_WRONGDEVICE;
    if (result < 0) goto error;

    card->cid = id;
    card->size = (u16)(id & 0xFCu);
    card->sectorSize = (s32)SectorSizeTable[(id & 0x00003800u) >> 11];
    card->cBlock = (u16)((card->size * 1024u * 1024u / 8u) / (u32)card->sectorSize);
    card->latency = (s32)LatencyTable[(id & 0x00000700u) >> 8];

    result = __CARDClearStatus(chan);
    if (result < 0) goto error;
    result = __CARDReadStatus(chan, &status);
    if (result < 0) goto error;
    if (!EXIProbe(chan)) { result = CARD_RESULT_NOCARD; goto error; }

    if ((status & 0x40u) == 0) {
      result = __CARDUnlock(chan, card->id);
      if (result < 0) goto error;
      checkSum = 0;
      sram = __OSLockSramEx();
      for (int i=0;i<12;i++) { sram->flashID[chan][i] = card->id[i]; checkSum = (u8)(checkSum + card->id[i]); }
      sram->flashIDCheckSum[chan] = (u8)~checkSum;
      __OSUnlockSramEx(TRUE);
      return result;
    }

    card->mountStep = 1;
    checkSum = 0;
    sram = __OSLockSramEx();
    for (int i=0;i<12;i++) checkSum = (u8)(checkSum + sram->flashID[chan][i]);
    __OSUnlockSramEx(FALSE);
    if (sram->flashIDCheckSum[chan] != (u8)~checkSum) { result = CARD_RESULT_IOERROR; goto error; }
  }

  if (card->mountStep == 1) {
    card->mountStep = 2;
    result = __CARDEnableInterrupt(chan, TRUE);
    if (result < 0) goto error;
    EXISetExiCallback(chan, 0);
    EXIUnlock(chan);
  }

  step = card->mountStep - 2;
  return __CARDRead(chan, (u32)card->sectorSize * (u32)step, CARD_SYSTEM_BLOCK_SIZE,
                    (u8*)card->workArea + (u32)CARD_SYSTEM_BLOCK_SIZE * (u32)step, __CARDMountCallback);

error:
  EXIUnlock(chan);
  DoUnmount(chan, result);
  return result;
}

s32 oracle_CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback) {
  CARDControl* card;
  if (chan < 0 || 2 <= chan) return CARD_RESULT_FATAL_ERROR;

  // GameChoice lowmem byte at 0x800030E3.
  volatile u8* gamechoice = (volatile u8*)0x800030E3u;
  if ((*gamechoice) & 0x80u) return CARD_RESULT_NOCARD;

  card = &__CARDBlock[chan];
  if (card->result == CARD_RESULT_BUSY) return CARD_RESULT_BUSY;
  if (!card->attached && (EXIGetState(chan) & EXI_STATE_ATTACHED)) return CARD_RESULT_WRONGDEVICE;

  card->result = CARD_RESULT_BUSY;
  card->workArea = workArea;
  card->extCallback = detachCallback;
  card->apiCallback = attachCallback ? attachCallback : __CARDDefaultApiCallback;
  card->exiCallback = 0;

  if (!card->attached && !EXIAttach(chan, __CARDExtHandler)) {
    card->result = CARD_RESULT_NOCARD;
    return CARD_RESULT_NOCARD;
  }

  card->mountStep = 0;
  card->attached = TRUE;
  EXISetExiCallback(chan, 0);
  card->currentDir = 0;
  card->currentFat = 0;

  card->unlockCallback = __CARDMountCallback;
  if (!EXILock(chan, 0, __CARDUnlockedHandler)) {
    return CARD_RESULT_READY;
  }
  card->unlockCallback = 0;
  return DoMount(chan);
}

s32 oracle_CARDMount(s32 chan, void* workArea, CARDCallback attachCallback) {
  s32 result = oracle_CARDMountAsync(chan, workArea, attachCallback, __CARDDefaultApiCallback);
  if (result < 0) {
    return result;
  }
  return oracle___CARDSync(chan);
}

// Helpers exported for the DOL test to configure oracles.
void oracle_mount_init_defaults(void) {
  memset(__CARDBlock, 0, sizeof(__CARDBlock));
  memset(&s_sram_ex, 0, sizeof(s_sram_ex));
  memset(oracle_exi_state, 0, sizeof(oracle_exi_state));
  memset(oracle_exi_exi_cb, 0, sizeof(oracle_exi_exi_cb));
  memset(oracle_exi_card_status, 0, sizeof(oracle_exi_card_status));
  memset(oracle_exi_card_status_reads, 0, sizeof(oracle_exi_card_status_reads));
  memset(oracle_exi_card_status_clears, 0, sizeof(oracle_exi_card_status_clears));
  memset(oracle_exi_getid_ok, 0, sizeof(oracle_exi_getid_ok));
  memset(oracle_exi_id, 0, sizeof(oracle_exi_id));
  s_sram_locked = 0;
  s_font_encode = 0;
  oracle_seed_card_img_valid();
}

void oracle_mount_seed_valid_card(int chan) {
  // Choose the same flashIDs as in other suites for determinism.
  for (int i = 0; i < 12; i++) {
    s_sram_ex.flashID[0][i] = (u8)(0x10u + (u8)i);
    s_sram_ex.flashID[1][i] = (u8)(0x80u + (u8)i);
  }
  // Ensure flashID checksum passes the step0 branch when status&0x40 != 0.
  u8 sum = 0;
  for (int i = 0; i < 12; i++) sum = (u8)(sum + s_sram_ex.flashID[chan][i]);
  s_sram_ex.flashIDCheckSum[chan] = (u8)~sum;

  make_valid_system_blocks(chan, 0x0004u, 64u, 0u);
}
