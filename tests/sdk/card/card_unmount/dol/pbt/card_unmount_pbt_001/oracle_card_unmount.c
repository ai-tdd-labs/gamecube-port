#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_NOCARD = -3,
  CARD_RESULT_FATAL_ERROR = -128,
};

typedef void (*CARDCallback)(s32 chan, s32 result);
typedef void (*EXICallback)(s32 chan, void* context);

enum { EXI_STATE_ATTACHED = 0x08 };

static u32 s_exi_state[2];
static EXICallback s_exi_cb[2];
static EXICallback s_ext_cb[2];

static EXICallback EXISetExiCallback(s32 chan, EXICallback cb) {
  if (chan < 0 || chan >= 2) return 0;
  EXICallback prev = s_exi_cb[chan];
  s_exi_cb[chan] = cb;
  return prev;
}

static BOOL EXIAttach(s32 chan, EXICallback cb) {
  if (chan < 0 || chan >= 2) return FALSE;
  s_exi_state[chan] |= EXI_STATE_ATTACHED;
  s_ext_cb[chan] = cb;
  return TRUE;
}

static BOOL EXIDetach(s32 chan) {
  if (chan < 0 || chan >= 2) return FALSE;
  s_exi_state[chan] &= ~EXI_STATE_ATTACHED;
  s_ext_cb[chan] = 0;
  return TRUE;
}

static u32 EXIGetState(s32 chan) {
  if (chan < 0 || chan >= 2) return 0;
  return s_exi_state[chan];
}

static int OSDisableInterrupts(void) { return 1; }
static void OSRestoreInterrupts(int enabled) { (void)enabled; }

typedef struct CARDControl {
  s32 result;
  void* diskID;
  BOOL attached;
  s32 mountStep;
  u32 alarm_cancel_calls;
  CARDCallback apiCallback;
} CARDControl;

static CARDControl __CARDBlock[2];

static void OSCancelAlarm(u32* calls) {
  if (calls) (*calls)++;
}

static s32 __CARDGetControlBlock(s32 chan, CARDControl** pcard) {
  BOOL enabled;
  s32 result;
  CARDControl* card;

  card = &__CARDBlock[chan];
  if (chan < 0 || chan >= 2 || card->diskID == 0) {
    return CARD_RESULT_FATAL_ERROR;
  }

  enabled = OSDisableInterrupts();
  if (!card->attached) {
    result = CARD_RESULT_NOCARD;
  } else if (card->result == CARD_RESULT_BUSY) {
    result = CARD_RESULT_BUSY;
  } else {
    card->result = CARD_RESULT_BUSY;
    card->apiCallback = 0;
    *pcard = card;
    result = CARD_RESULT_READY;
  }
  OSRestoreInterrupts(enabled);
  return result;
}

static void DoUnmount(s32 chan, s32 result) {
  CARDControl* card;
  BOOL enabled;

  card = &__CARDBlock[chan];
  enabled = OSDisableInterrupts();
  if (card->attached) {
    EXISetExiCallback(chan, 0);
    EXIDetach(chan);
    OSCancelAlarm(&card->alarm_cancel_calls);
    card->attached = FALSE;
    card->result = result;
    card->mountStep = 0;
  }
  OSRestoreInterrupts(enabled);
}

static s32 CARDUnmount(s32 chan) {
  CARDControl* card;
  s32 result;

  result = __CARDGetControlBlock(chan, &card);
  if (result < 0) {
    return result;
  }
  DoUnmount(chan, CARD_RESULT_NOCARD);
  return CARD_RESULT_READY;
}

// Case setup helpers
static void reset_state(void) {
  memset(__CARDBlock, 0, sizeof(__CARDBlock));
  memset(s_exi_state, 0, sizeof(s_exi_state));
  memset(s_exi_cb, 0, sizeof(s_exi_cb));
  memset(s_ext_cb, 0, sizeof(s_ext_cb));
}

static void init_card0_attached_ready(void) {
  __CARDBlock[0].diskID = (void*)0x80000000u;
  __CARDBlock[0].attached = TRUE;
  __CARDBlock[0].result = CARD_RESULT_READY;
  __CARDBlock[0].mountStep = 7;
  __CARDBlock[0].alarm_cancel_calls = 0;

  (void)EXIAttach(0, (EXICallback)(uintptr_t)0x11111111u);
  (void)EXISetExiCallback(0, (EXICallback)(uintptr_t)0x22222222u);
}

static void init_card0_unattached(void) {
  __CARDBlock[0].diskID = (void*)0x80000000u;
  __CARDBlock[0].attached = FALSE;
  __CARDBlock[0].result = CARD_RESULT_READY;
  __CARDBlock[0].mountStep = 5;

  s_exi_state[0] &= ~EXI_STATE_ATTACHED;
  s_exi_cb[0] = (EXICallback)(uintptr_t)0x33333333u;
  // ext callback only exists when attached; keep this 0 for parity with host model.
  s_ext_cb[0] = 0;
}

static void init_card0_busy(void) {
  __CARDBlock[0].diskID = (void*)0x80000000u;
  __CARDBlock[0].attached = TRUE;
  __CARDBlock[0].result = CARD_RESULT_BUSY;
  __CARDBlock[0].mountStep = 3;
  __CARDBlock[0].alarm_cancel_calls = 0;

  (void)EXIAttach(0, (EXICallback)(uintptr_t)0x55555555u);
  (void)EXISetExiCallback(0, (EXICallback)(uintptr_t)0x66666666u);
}

static inline void wr32be(volatile u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)(v);
}

static void dump_case(volatile u8* out, u32* off_words, u32 case_id, s32 ret, s32 chan) {
  u32 w = *off_words;
  CARDControl* c = &__CARDBlock[chan];

  wr32be(out + (w++ * 4u), case_id);
  wr32be(out + (w++ * 4u), (u32)ret);
  wr32be(out + (w++ * 4u), (u32)c->attached);
  wr32be(out + (w++ * 4u), (u32)c->result);
  wr32be(out + (w++ * 4u), (u32)c->mountStep);
  wr32be(out + (w++ * 4u), (u32)EXIGetState(chan));
  wr32be(out + (w++ * 4u), (u32)(uintptr_t)s_exi_cb[chan]);
  wr32be(out + (w++ * 4u), (u32)(uintptr_t)s_ext_cb[chan]);
  wr32be(out + (w++ * 4u), c->alarm_cancel_calls);

  *off_words = w;
}

void oracle_run_unmount_suite(volatile u8* out) {
  u32 w = 0;

  wr32be(out + 0, 0x43554E30u); // "CUN0"
  wr32be(out + 4, 3u);
  w = 2;

  // Case 0: attached+ready -> unmount
  reset_state();
  init_card0_attached_ready();
  s32 r0 = CARDUnmount(0);
  dump_case(out, &w, 0u, r0, 0);

  // Case 1: unattached -> NOCARD, no DoUnmount
  reset_state();
  init_card0_unattached();
  s32 r1 = CARDUnmount(0);
  dump_case(out, &w, 1u, r1, 0);

  // Case 2: attached but BUSY -> BUSY, no DoUnmount
  reset_state();
  init_card0_busy();
  s32 r2 = CARDUnmount(0);
  dump_case(out, &w, 2u, r2, 0);
}
