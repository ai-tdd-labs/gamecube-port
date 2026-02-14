#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

enum { EXI_READ = 0, EXI_WRITE = 1 };

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_NOCARD = -3,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_FATAL_ERROR = -128,
};

enum { CARD_SEG_SIZE = 512 };

typedef void (*CARDCallback)(s32 chan, s32 result);
typedef void (*EXICallback)(s32 chan, void* context);

typedef struct {
  BOOL attached;
  s32 result;
  s32 latency;
  void* workArea;

  u8 cmd[9];
  s32 cmdlen;
  u32 mode;
  int retry;

  int repeat;
  u32 addr;
  void* buffer;
  s32 xferred;

  CARDCallback txCallback;
  CARDCallback exiCallback;
  CARDCallback apiCallback;
  CARDCallback xferCallback;
  CARDCallback unlockCallback;
} CARDControl;

// Modeled global from SDK.
CARDControl __CARDBlock[2];

// Oracle entrypoints for the DOL test.
s32 oracle___CARDStart(s32 chan, CARDCallback txCallback, CARDCallback exiCallback);
s32 oracle___CARDReadSegment(s32 chan, CARDCallback callback);
s32 oracle___CARDRead(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback);

// Deterministic EXI oracle backing store.
static u8 s_card_img[0x40000];
static int s_selected;
static int s_has_addr;
static u32 s_addr;

// Deterministic knobs.
u32 oracle_exi_lock_ok = 1;
u32 oracle_exi_select_ok = 1;
u32 oracle_exi_dma_ok = 1;
u32 oracle_exi_probe_ok = 1;

// Instrumentation.
u32 oracle_exi_deselect_calls;
u32 oracle_exi_unlock_calls;
u32 oracle_card_tx_calls;

static u32 decode_addr(const u8* b5) {
  return (((u32)(b5[1] & 0x7Fu)) << 17) |
         (((u32)b5[2]) << 9) |
         (((u32)(b5[3] & 0x03u)) << 7) |
         ((u32)(b5[4] & 0x7Fu));
}

void oracle_EXIInit(void) {
  s_selected = 0;
  s_has_addr = 0;
  s_addr = 0;
  oracle_exi_deselect_calls = 0;
  oracle_exi_unlock_calls = 0;
  oracle_card_tx_calls = 0;

  // Deterministic fill (matches host-side exi_dma_card_proto suite pattern).
  for (u32 i = 0; i < (u32)sizeof(s_card_img); i++) {
    s_card_img[i] = (u8)(i ^ 0xA5u);
  }
  // Seed a known read window @0x21000.
  for (u32 i = 0; i < 512u; i++) {
    s_card_img[0x21000u + i] = (u8)(0xC0u ^ (u8)i);
  }
}

int oracle_EXILock(s32 chan, u32 dev, EXICallback cb) {
  (void)chan;
  (void)dev;
  (void)cb;
  return oracle_exi_lock_ok ? 1 : 0;
}
int oracle_EXIUnlock(s32 chan) {
  (void)chan;
  oracle_exi_unlock_calls++;
  return 1;
}
int oracle_EXISelect(s32 chan, u32 dev, u32 freq) {
  (void)chan;
  (void)dev;
  (void)freq;
  if (!oracle_exi_select_ok) return 0;
  s_selected = 1;
  return 1;
}
int oracle_EXIDeselect(s32 chan) {
  (void)chan;
  oracle_exi_deselect_calls++;
  s_selected = 0;
  return 1;
}
int oracle_EXIProbe(s32 chan) {
  (void)chan;
  return oracle_exi_probe_ok ? 1 : 0;
}

int oracle_EXIImmEx(s32 chan, void* buf, s32 len, u32 type) {
  (void)chan;
  if (type != EXI_READ && buf && len >= 5) {
    const u8* b = (const u8*)buf;
    if (b[0] == 0x52u || b[0] == 0xF2u || b[0] == 0xF1u) {
      s_addr = decode_addr(b);
      s_has_addr = 1;
    }
  }
  return 1;
}

int oracle_EXIDma(s32 chan, void* buffer, s32 length, u32 type, EXICallback cb) {
  (void)chan;
  if (!oracle_exi_dma_ok) return 0;
  if (!s_selected || !s_has_addr) return 0;
  if (!buffer || length <= 0) return 0;
  u32 len = (u32)length;
  if (s_addr + len > (u32)sizeof(s_card_img)) return 0;

  if (type == EXI_READ) {
    memcpy(buffer, &s_card_img[s_addr], len);
  } else if (type == EXI_WRITE) {
    memcpy(&s_card_img[s_addr], buffer, len);
  } else {
    return 0;
  }

  // Deterministic: complete transfer immediately.
  if (cb) cb(chan, 0);
  return 1;
}

static int OSDisableInterrupts(void) { return 1; }
static int OSRestoreInterrupts(int level) { return level; }

static s32 oracle___CARDPutControlBlock(CARDControl* card, s32 result) {
  int enabled = OSDisableInterrupts();
  if (card->attached) {
    card->result = result;
  } else if (card->result == CARD_RESULT_BUSY) {
    card->result = result;
  }
  OSRestoreInterrupts(enabled);
  return result;
}

// MP4 decomp: __CARDTxHandler finishes the EXI transfer and calls txCallback.
static void oracle___CARDTxHandler(s32 chan, void* context) {
  (void)context;
  CARDControl* card = &__CARDBlock[chan];
  CARDCallback callback;
  BOOL err;

  err = !oracle_EXIDeselect(chan);
  oracle_EXIUnlock(chan);

  callback = card->txCallback;
  if (callback) {
    card->txCallback = 0;
    oracle_card_tx_calls++;
    callback(chan, (!err && oracle_EXIProbe(chan)) ? CARD_RESULT_READY : CARD_RESULT_NOCARD);
  }
}

static void BlockReadCallback(s32 chan, s32 result) {
  CARDControl* card = &__CARDBlock[chan];
  CARDCallback callback;

  if (result < 0) goto error;

  card->xferred += CARD_SEG_SIZE;
  card->addr += CARD_SEG_SIZE;
  card->buffer = (void*)((uintptr_t)card->buffer + CARD_SEG_SIZE);

  if (--card->repeat <= 0) goto error;

  result = 0; // will be overwritten by next segment start
  result = oracle___CARDReadSegment(chan, BlockReadCallback);
  if (result < 0) goto error;
  return;

error:
  if (card->apiCallback == 0) {
    oracle___CARDPutControlBlock(card, result);
  }
  callback = card->xferCallback;
  if (callback) {
    card->xferCallback = 0;
    callback(chan, result);
  }
}

s32 oracle___CARDStart(s32 chan, CARDCallback txCallback, CARDCallback exiCallback) {
  int enabled;
  CARDControl* card;
  s32 result;

  if (chan < 0 || chan >= 2) return CARD_RESULT_FATAL_ERROR;

  enabled = OSDisableInterrupts();
  card = &__CARDBlock[chan];
  if (!card->attached) {
    result = CARD_RESULT_NOCARD;
  } else {
    if (txCallback) card->txCallback = txCallback;
    if (exiCallback) card->exiCallback = exiCallback;
    card->unlockCallback = 0;
    if (!oracle_EXILock(chan, 0, 0)) {
      result = CARD_RESULT_BUSY;
    } else if (!oracle_EXISelect(chan, 0, 4)) {
      oracle_EXIUnlock(chan);
      result = CARD_RESULT_NOCARD;
    } else {
      // SetupTimeoutAlarm(card) -- not modeled
      result = CARD_RESULT_READY;
    }
  }
  OSRestoreInterrupts(enabled);
  return result;
}

#define AD1(x) ((u8)(((x) >> 17) & 0x7f))
#define AD2(x) ((u8)(((x) >> 9) & 0xff))
#define AD3(x) ((u8)(((x) >> 7) & 0x03))
#define BA(x) ((u8)((x)&0x7f))

// Minimal "CARDID" size (only used for workArea+sizeof(CARDID) pointer math).
typedef struct { u8 _pad[512]; } CARDID;

s32 oracle___CARDReadSegment(s32 chan, CARDCallback callback) {
  CARDControl* card;
  s32 result;

  if (chan < 0 || chan >= 2) return CARD_RESULT_FATAL_ERROR;

  card = &__CARDBlock[chan];
  card->cmd[0] = 0x52;
  card->cmd[1] = AD1(card->addr);
  card->cmd[2] = AD2(card->addr);
  card->cmd[3] = AD3(card->addr);
  card->cmd[4] = BA(card->addr);
  card->cmdlen = 5;
  card->mode = 0;
  card->retry = 0;

  result = oracle___CARDStart(chan, callback, 0);
  if (result == CARD_RESULT_BUSY) {
    return CARD_RESULT_READY;
  }
  if (result < 0) return result;

  if (!oracle_EXIImmEx(chan, card->cmd, card->cmdlen, EXI_WRITE) ||
      !oracle_EXIImmEx(chan, (u8*)card->workArea + sizeof(CARDID), card->latency, EXI_WRITE) ||
      !oracle_EXIDma(chan, card->buffer, 512, EXI_READ, oracle___CARDTxHandler)) {
    card->txCallback = 0;
    oracle_EXIDeselect(chan);
    oracle_EXIUnlock(chan);
    return CARD_RESULT_NOCARD;
  }
  return CARD_RESULT_READY;
}

s32 oracle___CARDRead(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback) {
  CARDControl* card;
  if (chan < 0 || chan >= 2) return CARD_RESULT_FATAL_ERROR;
  card = &__CARDBlock[chan];
  if (!card->attached) {
    return CARD_RESULT_NOCARD;
  }
  card->xferCallback = callback;
  card->repeat = (int)(length / CARD_SEG_SIZE);
  card->addr = addr;
  card->buffer = dst;
  return oracle___CARDReadSegment(chan, BlockReadCallback);
}
