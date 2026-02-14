#include <stdint.h>

#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/exi.h"

#include "card/card_bios.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_NOCARD = -3,
};

// SDK
void CARDInit(void);
s32 CARDUnmount(s32 chan);
void EXIInit(void);

extern uintptr_t gc_exi_exi_callback_ptr[3];
extern uintptr_t gc_exi_ext_callback_ptr[3];

static void dump_case(u8* out, u32* w, u32 case_id, s32 ret, s32 chan) {
  GcCardControl* c = &gc_card_block[chan];

  wr32be(out + ((*w)++ * 4u), case_id);
  wr32be(out + ((*w)++ * 4u), (u32)ret);
  wr32be(out + ((*w)++ * 4u), (u32)c->attached);
  wr32be(out + ((*w)++ * 4u), (u32)c->result);
  wr32be(out + ((*w)++ * 4u), (u32)c->mount_step);
  wr32be(out + ((*w)++ * 4u), (u32)EXIGetState(chan));
  wr32be(out + ((*w)++ * 4u), (u32)gc_exi_exi_callback_ptr[chan]);
  wr32be(out + ((*w)++ * 4u), (u32)gc_exi_ext_callback_ptr[chan]);
  wr32be(out + ((*w)++ * 4u), c->alarm_cancel_calls);
}

static void setup_attached_ready(s32 chan) {
  EXIInit();
  CARDInit();
  gc_card_block[chan].attached = 1;
  gc_card_block[chan].result = CARD_RESULT_READY;
  gc_card_block[chan].mount_step = 7;
  gc_card_block[chan].alarm_cancel_calls = 0;

  (void)EXIAttach(chan, (EXICallback)(uintptr_t)0x11111111u);
  (void)EXISetExiCallback(chan, (EXICallback)(uintptr_t)0x22222222u);
}

static void setup_unattached(s32 chan) {
  EXIInit();
  CARDInit();
  gc_card_block[chan].attached = 0;
  gc_card_block[chan].result = CARD_RESULT_READY;
  gc_card_block[chan].mount_step = 5;
  gc_card_block[chan].alarm_cancel_calls = 0;

  (void)EXISetExiCallback(chan, (EXICallback)(uintptr_t)0x33333333u);
}

static void setup_busy(s32 chan) {
  EXIInit();
  CARDInit();
  gc_card_block[chan].attached = 1;
  gc_card_block[chan].result = CARD_RESULT_BUSY;
  gc_card_block[chan].mount_step = 3;
  gc_card_block[chan].alarm_cancel_calls = 0;

  (void)EXIAttach(chan, (EXICallback)(uintptr_t)0x55555555u);
  (void)EXISetExiCallback(chan, (EXICallback)(uintptr_t)0x66666666u);
}

const char* gc_scenario_label(void) { return "CARDUnmount/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_unmount_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
  (void)ram;

  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x100);
  if (!out) die("gc_ram_ptr failed");
  for (u32 i = 0; i < 0x100u; i++) out[i] = 0;

  wr32be(out + 0, 0x43554E30u); // "CUN0"
  wr32be(out + 4, 3u);

  u32 w = 2;

  // Case 0
  setup_attached_ready(0);
  s32 r0 = CARDUnmount(0);
  dump_case(out, &w, 0u, r0, 0);

  // Case 1
  setup_unattached(0);
  s32 r1 = CARDUnmount(0);
  dump_case(out, &w, 1u, r1, 0);

  // Case 2
  setup_busy(0);
  s32 r2 = CARDUnmount(0);
  dump_case(out, &w, 2u, r2, 0);
}
