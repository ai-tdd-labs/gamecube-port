#include <stdint.h>

typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

typedef struct OSThreadQueue { void* head; void* tail; } OSThreadQueue;

static u32 s_sleep_calls;
static u32 s_wakeup_calls;

void (*gc_os_sleep_hook)(void* queue);

void OSSleepThread(OSThreadQueue* q) {
  s_sleep_calls++;
  if (gc_os_sleep_hook) gc_os_sleep_hook((void*)q);
}
void OSWakeupThread(OSThreadQueue* q) { (void)q; s_wakeup_calls++; }

static int OSDisableInterrupts(void) { return 1; }
static int OSRestoreInterrupts(int level) { return level; }

enum { CARD_RESULT_BUSY = -1, CARD_RESULT_FATAL_ERROR = -128 };

typedef struct {
  s32 result;
  u32 thread_queue_inited;
  int attached;
} GcCardControl;

GcCardControl gc_card_block[2];

static s32 CARDGetResultCode(s32 chan) {
  if (chan < 0 || chan >= 2) return CARD_RESULT_FATAL_ERROR;
  return gc_card_block[chan].result;
}

s32 oracle___CARDSync(s32 chan) {
  s32 result;
  int enabled;
  GcCardControl* block;
  if (chan < 0 || chan >= 2) return CARD_RESULT_FATAL_ERROR;
  block = &gc_card_block[chan];
  enabled = OSDisableInterrupts();
  while ((result = CARDGetResultCode(chan)) == CARD_RESULT_BUSY) {
    OSSleepThread((OSThreadQueue*)&block->thread_queue_inited);
  }
  OSRestoreInterrupts(enabled);
  return result;
}

u32 oracle_get_sleep_calls(void) { return s_sleep_calls; }
u32 oracle_get_wakeup_calls(void) { return s_wakeup_calls; }
