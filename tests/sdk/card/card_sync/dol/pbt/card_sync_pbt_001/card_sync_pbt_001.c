#include <stdint.h>
#include <string.h>
#include "src/sdk_port/gc_mem.c"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

extern s32 oracle___CARDSync(s32 chan);
extern u32 oracle_get_sleep_calls(void);
extern void (*gc_os_sleep_hook)(void* queue);

typedef struct { s32 result; u32 thread_queue_inited; int attached; } GcCardControl;
extern GcCardControl gc_card_block[2];

enum { CARD_RESULT_BUSY = -1 };

static inline void wr32be(volatile u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}
static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline u32 h32(u32 h, u32 v) { return rotl1(h) ^ v; }

static void sleep_hook(void* q) {
  (void)q;
  // Flip to READY on first sleep.
  gc_card_block[0].result = 0;
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (u8*)0x80000000u);
  volatile u8* out = (volatile u8*)0x80300000u;
  for (u32 i = 0; i < 0x40u; i++) out[i] = 0;

  u32 l0 = 0;
  memset(gc_card_block, 0, sizeof(gc_card_block));

  // Case A: READY returns immediately.
  gc_card_block[0].result = 0;
  l0 = h32(l0, (u32)oracle___CARDSync(0));
  l0 = h32(l0, oracle_get_sleep_calls());

  // Case B: BUSY causes a sleep attempt; we flip to READY on first sleep.
  gc_os_sleep_hook = sleep_hook;
  gc_card_block[0].result = CARD_RESULT_BUSY;
  l0 = h32(l0, (u32)oracle___CARDSync(0));
  l0 = h32(l0, oracle_get_sleep_calls());
  gc_os_sleep_hook = 0;

  // Case C: out of range.
  l0 = h32(l0, (u32)oracle___CARDSync(-1));
  l0 = h32(l0, (u32)oracle___CARDSync(2));

  wr32be(out + 0x00, 0x4353594Eu); // "CSYN"
  wr32be(out + 0x04, l0);

  while (1) {}
  return 0;
}
