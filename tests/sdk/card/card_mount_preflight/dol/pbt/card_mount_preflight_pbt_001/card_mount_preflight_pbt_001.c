#include <stdint.h>
#include <string.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef int32_t s32;
typedef uint8_t u8;

typedef void (*CARDCallback)(s32 chan, s32 result);

extern s32 gc_exi_probeex_ret[3];
extern u32 gc_exi_getid_ok[3];
extern u32 gc_exi_id[3];
extern u32 gc_exi_attach_ok[3];
extern u32 gc_exi_state[3];
extern u32 gc_exi_lock_held[3];

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
extern GcCardControl gc_card_block[2];

s32 oracle_CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback);

static inline void be(volatile u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static u32 fnv1a32_u32(u32 h, u32 v) {
  h ^= (v >> 24) & 0xFFu; h *= 16777619u;
  h ^= (v >> 16) & 0xFFu; h *= 16777619u;
  h ^= (v >> 8) & 0xFFu; h *= 16777619u;
  h ^= v & 0xFFu; h *= 16777619u;
  return h;
}

static void poison(u32 s) {
  *(volatile u8*)0x800030E3u = 0;
  for (int i = 0; i < 3; i++) {
    gc_exi_probeex_ret[i] = -1;
    gc_exi_getid_ok[i] = 0;
    gc_exi_id[i] = 0;
    gc_exi_attach_ok[i] = 1;
    gc_exi_state[i] = 0;
    gc_exi_lock_held[i] = 0;
  }
  for (int c = 0; c < 2; c++) {
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

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (u8*)0x80000000u);
  volatile u8* out = (volatile u8*)0x80300000u;
  u32 off = 0, tc = 0, m = 0;

  // L0: explicit branches (avoid DoMount by holding lock when needed).
  u32 a = 0;
  poison(0); a = rotl1(a) ^ snap(0, oracle_CARDMountAsync(-1, (void*)0x1, dummy_cb, dummy_cb), dummy_cb, dummy_cb);
  poison(1); a = rotl1(a) ^ snap(0, oracle_CARDMountAsync(2, (void*)0x1, dummy_cb, dummy_cb), dummy_cb, dummy_cb);

  poison(2); *(volatile u8*)0x800030E3u = 0x80; a = rotl1(a) ^ snap(0, oracle_CARDMountAsync(0, (void*)0x1234, dummy_cb, 0), dummy_cb, 0);

  poison(3); gc_card_block[0].result = -1; a = rotl1(a) ^ snap(0, oracle_CARDMountAsync(0, (void*)0x1234, 0, 0), 0, 0);

  poison(4); gc_card_block[0].attached = 0; gc_exi_state[0] = 0x08; a = rotl1(a) ^ snap(0, oracle_CARDMountAsync(0, (void*)0x1234, 0, 0), 0, 0);

  poison(5); gc_card_block[0].attached = 0; gc_exi_attach_ok[0] = 0; a = rotl1(a) ^ snap(0, oracle_CARDMountAsync(0, (void*)0x1234, dummy_cb, 0), dummy_cb, 0);

  poison(6); gc_exi_lock_held[0] = 1; a = rotl1(a) ^ snap(0, oracle_CARDMountAsync(0, (void*)0x1234, dummy_cb, 0), dummy_cb, 0);

  poison(7); gc_card_block[0].attached = 1; gc_exi_attach_ok[0] = 0; gc_exi_lock_held[0] = 1;
  a = rotl1(a) ^ snap(0, oracle_CARDMountAsync(0, (void*)0x5678, 0, dummy_cb), 0, dummy_cb);

  tc += 8; m = rotl1(m) ^ a;

  // L1: accumulation of attach failures vs lock failures.
  u32 b = 0;
  for (u32 i = 0; i < 64; i++) {
    poison(0x100u + i);
    gc_card_block[0].attached = 0;
    gc_exi_attach_ok[0] = (i & 1u) ? 0u : 1u;
    gc_exi_lock_held[0] = 1; // force return READY if attach succeeds
    b = rotl1(b) ^ snap(0, oracle_CARDMountAsync(0, (void*)(uintptr_t)(0x1000u + i), dummy_cb, 0), dummy_cb, 0);
  }
  tc += 64; m = rotl1(m) ^ b;

  // L2: overwrite/idempotency: repeated calls should keep attached=1 once set.
  u32 c = 0;
  poison(0x200u);
  gc_exi_lock_held[0] = 1;
  for (u32 i = 0; i < 16; i++) {
    gc_exi_attach_ok[0] = (i == 0) ? 1u : 0u;
    c = rotl1(c) ^ snap(0, oracle_CARDMountAsync(0, (void*)0x2222, dummy_cb, 0), dummy_cb, 0);
  }
  tc += 16; m = rotl1(m) ^ c;

  // L3: random-start deterministic.
  u32 d = 0;
  rs = 0xA11CE5E1u;
  for (u32 i = 0; i < 256; i++) {
    u32 s = rn();
    poison(s);
    int chan = (int)(rn() % 2u);
    if (rn() & 1u) *(volatile u8*)0x800030E3u = 0x80;
    if (rn() & 1u) gc_card_block[chan].result = -1;
    if (rn() & 1u) { gc_card_block[chan].attached = 0; gc_exi_state[chan] = 0x08; }
    gc_exi_attach_ok[chan] = (rn() & 1u) ? 1u : 0u;
    gc_exi_lock_held[chan] = 1;
    d = rotl1(d) ^ snap(chan, oracle_CARDMountAsync(chan, (void*)(uintptr_t)(0x9000u + i), dummy_cb, dummy_cb), dummy_cb, dummy_cb);
  }
  tc += 256; m = rotl1(m) ^ d;

  // L4: callsite-ish: attach ok then lock held => READY.
  u32 e = 0;
  poison(0x400u);
  gc_exi_lock_held[0] = 1;
  for (u32 i = 0; i < 4; i++) e = rotl1(e) ^ snap(0, oracle_CARDMountAsync(0, (void*)0xABCD, dummy_cb, 0), dummy_cb, 0);
  tc += 4; m = rotl1(m) ^ e;

  // L5: boundary: attachCallback NULL chooses default API cb pointer.
  u32 f = 0;
  poison(0x500u);
  gc_exi_lock_held[0] = 1;
  f = rotl1(f) ^ snap(0, oracle_CARDMountAsync(0, (void*)0xDEAD, 0, 0), 0, 0);
  tc += 1; m = rotl1(m) ^ f;

  be(out + off * 4u, 0x434D5052u); off++; // "CMPR"
  be(out + off * 4u, tc); off++;
  be(out + off * 4u, m); off++;
  be(out + off * 4u, a); off++;
  be(out + off * 4u, b); off++;
  be(out + off * 4u, c); off++;
  be(out + off * 4u, d); off++;
  be(out + off * 4u, e); off++;
  be(out + off * 4u, f); off++;

  while (1) { __asm__ volatile("nop"); }
  return 0;
}
