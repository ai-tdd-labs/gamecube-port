#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_port/card/card_bios.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

s32 __CARDSync(s32 chan);

extern u32 gc_os_sleep_calls;
extern void (*gc_os_sleep_hook)(void* queue);

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline u32 h32(u32 h, u32 v) { return rotl1(h) ^ v; }

enum { CARD_RESULT_BUSY = -1 };

static void sleep_hook(void* q) {
  (void)q;
  // Flip to READY on first sleep.
  gc_card_block[0].result = 0;
}

const char* gc_scenario_label(void) { return "__CARDSync/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_sync_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x40u);
  if (!out) die("gc_ram_ptr failed");
  memset(out, 0, 0x40u);

  u32 l0 = 0;
  memset(gc_card_block, 0, sizeof(gc_card_block));
  gc_os_sleep_calls = 0;

  gc_card_block[0].result = 0;
  l0 = h32(l0, (u32)__CARDSync(0));
  l0 = h32(l0, gc_os_sleep_calls);

  gc_os_sleep_hook = sleep_hook;
  gc_card_block[0].result = CARD_RESULT_BUSY;
  l0 = h32(l0, (u32)__CARDSync(0));
  l0 = h32(l0, gc_os_sleep_calls);
  gc_os_sleep_hook = 0;

  l0 = h32(l0, (u32)__CARDSync(-1));
  l0 = h32(l0, (u32)__CARDSync(2));

  wr32be(out + 0x00, 0x4353594Eu);
  wr32be(out + 0x04, l0);
}
