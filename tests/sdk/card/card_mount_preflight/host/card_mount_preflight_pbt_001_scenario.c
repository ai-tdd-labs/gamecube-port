#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/types.h"
#include "dolphin/exi.h"

#include "sdk_port/card/card_bios.h"

typedef uint32_t u32;
typedef int32_t s32;
typedef uint8_t u8;

typedef void (*CARDCallback)(s32 chan, s32 result);

// CARD result codes (evidence: MP4 disasm @ 0x800D50A4 uses same enum set).
enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_WRONGDEVICE = -2,
  CARD_RESULT_NOCARD = -3,
  CARD_RESULT_FATAL_ERROR = -128,
};

s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback);

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static u32 fnv1a32_u32(u32 h, u32 v) {
  h ^= (v >> 24) & 0xFFu; h *= 16777619u;
  h ^= (v >> 16) & 0xFFu; h *= 16777619u;
  h ^= (v >> 8) & 0xFFu; h *= 16777619u;
  h ^= v & 0xFFu; h *= 16777619u;
  return h;
}

static void poison(GcRam* ram, u32 s) {
  *gc_ram_ptr(ram, 0x800030E3u, 1) = 0;
  EXIInit();
  for (int c = 0; c < GC_CARD_CHANS; c++) {
    memset(&gc_card_block[c], 0, sizeof(gc_card_block[c]));
    gc_card_block[c].result = (s32)(0x11110000u ^ (s + (u32)c));
    gc_card_block[c].attached = (s & 1u) ? 1 : 0;
    gc_card_block[c].mount_step = (s32)(0x22220000u ^ s);
    gc_card_block[c].work_area = (uintptr_t)(0x80000000u ^ s);
    gc_card_block[c].ext_callback = (uintptr_t)(0x81000000u ^ s);
    gc_card_block[c].api_callback = (uintptr_t)(0x82000000u ^ s);
    gc_card_block[c].unlock_callback = (uintptr_t)(0x83000000u ^ s);
    gc_card_block[c].alarm_cancel_calls = 0x44440000u ^ s;
    gc_card_block[c].current_dir = (s32)(0x55550000u ^ s);
    gc_card_block[c].current_fat = (s32)(0x66660000u ^ s);
  }

  // Reset EXI knobs.
  for (int i = 0; i < 3; i++) {
    gc_exi_probeex_ret[i] = -1;
    gc_exi_getid_ok[i] = 0;
    gc_exi_id[i] = 0;
    gc_exi_attach_ok[i] = 1;
  }
}

static u32 snap(int chan, s32 ret, CARDCallback detachCb, CARDCallback attachCb) {
  u32 h = 2166136261u;
  h = fnv1a32_u32(h, (u32)ret);
  h = fnv1a32_u32(h, (u32)gc_card_block[chan].result);
  h = fnv1a32_u32(h, (u32)gc_card_block[chan].attached);
  h = fnv1a32_u32(h, (u32)gc_card_block[chan].mount_step);
  h = fnv1a32_u32(h, (u32)gc_card_block[chan].work_area);
  // Do not hash raw function pointers (host/DOL addresses differ). Hash relations instead.
  h = fnv1a32_u32(h, (u32)(gc_card_block[chan].ext_callback != 0));
  h = fnv1a32_u32(h, (u32)(detachCb && gc_card_block[chan].ext_callback == (uintptr_t)detachCb));
  h = fnv1a32_u32(h, (u32)(gc_card_block[chan].api_callback != 0));
  h = fnv1a32_u32(h, (u32)(attachCb && gc_card_block[chan].api_callback == (uintptr_t)attachCb));
  h = fnv1a32_u32(h, (u32)(gc_card_block[chan].unlock_callback != 0));
  h = fnv1a32_u32(h, (u32)gc_card_block[chan].alarm_cancel_calls);
  h = fnv1a32_u32(h, (u32)gc_card_block[chan].current_dir);
  h = fnv1a32_u32(h, (u32)gc_card_block[chan].current_fat);
  return h;
}

static void dummy_cb(s32 chan, s32 result) { (void)chan; (void)result; }

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

const char* gc_scenario_label(void) { return "card_mount_preflight_pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_mount_preflight_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x40);
  u32 off = 0, tc = 0, m = 0;

  u32 a = 0;
  poison(ram, 0); a = rotl1(a) ^ snap(0, CARDMountAsync(-1, (void*)0x1, dummy_cb, dummy_cb), dummy_cb, dummy_cb);
  poison(ram, 1); a = rotl1(a) ^ snap(0, CARDMountAsync(2, (void*)0x1, dummy_cb, dummy_cb), dummy_cb, dummy_cb);

  poison(ram, 2); *gc_ram_ptr(ram, 0x800030E3u, 1) = 0x80; a = rotl1(a) ^ snap(0, CARDMountAsync(0, (void*)0x1234, dummy_cb, 0), dummy_cb, 0);

  poison(ram, 3); gc_card_block[0].result = CARD_RESULT_BUSY; a = rotl1(a) ^ snap(0, CARDMountAsync(0, (void*)0x1234, 0, 0), 0, 0);

  poison(ram, 4); gc_card_block[0].attached = 0; EXIAttach(0, 0); a = rotl1(a) ^ snap(0, CARDMountAsync(0, (void*)0x1234, 0, 0), 0, 0);

  poison(ram, 5); gc_card_block[0].attached = 0; gc_exi_attach_ok[0] = 0; a = rotl1(a) ^ snap(0, CARDMountAsync(0, (void*)0x1234, dummy_cb, 0), dummy_cb, 0);

  poison(ram, 6); (void)EXILock(0, 0, 0); a = rotl1(a) ^ snap(0, CARDMountAsync(0, (void*)0x1234, dummy_cb, 0), dummy_cb, 0);

  poison(ram, 7); gc_card_block[0].attached = 1; gc_exi_attach_ok[0] = 0; (void)EXILock(0, 0, 0);
  a = rotl1(a) ^ snap(0, CARDMountAsync(0, (void*)0x5678, 0, dummy_cb), 0, dummy_cb);

  tc += 8; m = rotl1(m) ^ a;

  u32 b = 0;
  for (u32 i = 0; i < 64; i++) {
    poison(ram, 0x100u + i);
    gc_card_block[0].attached = 0;
    gc_exi_attach_ok[0] = (i & 1u) ? 0u : 1u;
    (void)EXILock(0, 0, 0);
    b = rotl1(b) ^ snap(0, CARDMountAsync(0, (void*)(uintptr_t)(0x1000u + i), dummy_cb, 0), dummy_cb, 0);
  }
  tc += 64; m = rotl1(m) ^ b;

  u32 c = 0;
  poison(ram, 0x200u);
  (void)EXILock(0, 0, 0);
  for (u32 i = 0; i < 16; i++) {
    gc_exi_attach_ok[0] = (i == 0) ? 1u : 0u;
    c = rotl1(c) ^ snap(0, CARDMountAsync(0, (void*)0x2222, dummy_cb, 0), dummy_cb, 0);
  }
  tc += 16; m = rotl1(m) ^ c;

  u32 d = 0;
  rs = 0xA11CE5E1u;
  for (u32 i = 0; i < 256; i++) {
    u32 s = rn();
    poison(ram, s);
    int chan = (int)(rn() % 2u);
    if (rn() & 1u) *gc_ram_ptr(ram, 0x800030E3u, 1) = 0x80;
    if (rn() & 1u) gc_card_block[chan].result = CARD_RESULT_BUSY;
    if (rn() & 1u) { gc_card_block[chan].attached = 0; EXIAttach(chan, 0); }
    gc_exi_attach_ok[chan] = (rn() & 1u) ? 1u : 0u;
    (void)EXILock(chan, 0, 0);
    d = rotl1(d) ^ snap(chan, CARDMountAsync(chan, (void*)(uintptr_t)(0x9000u + i), dummy_cb, dummy_cb), dummy_cb, dummy_cb);
  }
  tc += 256; m = rotl1(m) ^ d;

  u32 e = 0;
  poison(ram, 0x400u);
  (void)EXILock(0, 0, 0);
  for (u32 i = 0; i < 4; i++) e = rotl1(e) ^ snap(0, CARDMountAsync(0, (void*)0xABCD, dummy_cb, 0), dummy_cb, 0);
  tc += 4; m = rotl1(m) ^ e;

  u32 f = 0;
  poison(ram, 0x500u);
  (void)EXILock(0, 0, 0);
  f = rotl1(f) ^ snap(0, CARDMountAsync(0, (void*)0xDEAD, 0, 0), 0, 0);
  tc += 1; m = rotl1(m) ^ f;

  wr32be(out + off * 4u, 0x434D5052u); off++;
  wr32be(out + off * 4u, tc); off++;
  wr32be(out + off * 4u, m); off++;
  wr32be(out + off * 4u, a); off++;
  wr32be(out + off * 4u, b); off++;
  wr32be(out + off * 4u, c); off++;
  wr32be(out + off * 4u, d); off++;
  wr32be(out + off * 4u, e); off++;
  wr32be(out + off * 4u, f); off++;
}
