#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/types.h"
#include "dolphin/exi.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

s32 __CARDEnableInterrupt(s32 chan, BOOL enable);

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline u32 h32(u32 h, u32 v) { return rotl1(h) ^ v; }

const char* gc_scenario_label(void) { return "__CARDEnableInterrupt/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_enable_interrupt_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x40u);
  if (!out) die("gc_ram_ptr failed");
  memset(out, 0, 0x40u);

  u32 l0 = 0, l1 = 0;

  EXIInit();
  (void)EXILock(0, 0, 0);
  l0 = h32(l0, (u32)__CARDEnableInterrupt(0, 1));
  l0 = h32(l0, gc_exi_last_imm_len[0]);
  l0 = h32(l0, gc_exi_last_imm_type[0]);
  l0 = h32(l0, gc_exi_last_imm_data[0]);

  l0 = h32(l0, (u32)__CARDEnableInterrupt(0, 0));
  l0 = h32(l0, gc_exi_last_imm_data[0]);
  (void)EXIUnlock(0);

  EXIInit();
  // No lock: EXISelect must fail.
  l1 = h32(l1, (u32)__CARDEnableInterrupt(0, 1));

  u32 master = 0;
  master = h32(master, l0);
  master = h32(master, l1);

  wr32be(out + 0x00, 0x4345494Eu); // "CEIN"
  wr32be(out + 0x04, master);
  wr32be(out + 0x08, l0);
  wr32be(out + 0x0C, l1);
}
