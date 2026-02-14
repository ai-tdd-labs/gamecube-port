#include <stdint.h>
#include <string.h>

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

typedef void (*CARDCallback)(s32 chan, s32 result);
typedef void (*EXICallback)(s32 chan, void* context);

enum { EXI_STATE_ATTACHED = 0x08 };

// Oracle EXI knobs.
s32 gc_exi_probeex_ret[3] = { -1, -1, -1 };
u32 gc_exi_getid_ok[3];
u32 gc_exi_id[3];
u32 gc_exi_attach_ok[3] = { 1, 1, 1 };
u32 gc_exi_state[3];
u32 gc_exi_lock_held[3];
EXICallback gc_exi_exi_cb[3];

static int OSDisableInterrupts(void) { return 1; }
static void OSRestoreInterrupts(int enabled) { (void)enabled; }

static void __CARDDefaultApiCallback(s32 chan, s32 result) { (void)chan; (void)result; }
static void __CARDExtHandler(s32 chan, void* context) { (void)chan; (void)context; }
static void __CARDUnlockedHandler(s32 chan, void* context) { (void)chan; (void)context; }
static void __CARDMountCallback(s32 chan, s32 result) { (void)chan; (void)result; }

static EXICallback EXISetExiCallback(s32 channel, EXICallback cb) {
  if (channel < 0 || channel >= 3) return 0;
  EXICallback prev = gc_exi_exi_cb[channel];
  gc_exi_exi_cb[channel] = cb;
  return prev;
}

static u32 EXIGetState(s32 channel) {
  if (channel < 0 || channel >= 3) return 0;
  return gc_exi_state[channel];
}

static int EXIAttach(s32 channel, EXICallback cb) {
  if (channel < 0 || channel >= 3) return 0;
  if (!gc_exi_attach_ok[channel]) return 0;
  gc_exi_state[channel] |= EXI_STATE_ATTACHED;
  (void)cb;
  return 1;
}

static int EXILock(s32 channel, u32 device, EXICallback cb) {
  (void)device;
  (void)cb;
  if (channel < 0 || channel >= 3) return 0;
  if (gc_exi_lock_held[channel]) return 0;
  gc_exi_lock_held[channel] = 1;
  return 1;
}

typedef struct {
  s32 result;
  s32 attached;
  s32 mount_step;
  uintptr_t work_area;
  uintptr_t ext_callback;
  uintptr_t api_callback;
  uintptr_t exi_callback;
  uintptr_t unlock_callback;
  u32 alarm_cancel_calls;
  s32 current_dir;
  s32 current_fat;
} GcCardControl;

GcCardControl gc_card_block[2];

static void OSCancelAlarm(u32* calls) { if (calls) (*calls)++; }

static s32 DoMount(s32 chan) { (void)chan; return CARD_RESULT_FATAL_ERROR; }

s32 oracle_CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback) {
  GcCardControl* card;
  int enabled;

  if (chan < 0 || 2 <= chan) return CARD_RESULT_FATAL_ERROR;

  volatile u8* gamechoice = (volatile u8*)0x800030E3u;
  if ((*gamechoice) & 0x80u) return CARD_RESULT_NOCARD;

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

  if (!card->attached && !EXIAttach(chan, __CARDExtHandler)) {
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
  if (!EXILock(chan, 0, __CARDUnlockedHandler)) {
    return CARD_RESULT_READY;
  }
  card->unlock_callback = 0;
  return DoMount(chan);
}

