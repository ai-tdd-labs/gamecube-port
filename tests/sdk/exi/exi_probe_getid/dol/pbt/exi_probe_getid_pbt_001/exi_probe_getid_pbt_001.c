#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef int32_t s32;

extern s32 gc_exi_probeex_ret[3];
extern u32 gc_exi_getid_ok[3];
extern u32 gc_exi_id[3];

s32 oracle_EXIProbeEx(s32 channel);
u32 oracle_EXIProbe(s32 channel);
s32 oracle_EXIGetID(s32 channel, u32 device, u32* id);

static inline void be(volatile uint8_t *p, u32 v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static void reset_cfg(u32 s) {
  for (int i = 0; i < 3; i++) {
    gc_exi_probeex_ret[i] = -1;
    gc_exi_getid_ok[i] = 0;
    gc_exi_id[i] = 0;
  }
  if (s & 1u) {
    gc_exi_probeex_ret[0] = 1;
    gc_exi_getid_ok[0] = 1;
    gc_exi_id[0] = 0x00000004u;
  }
  if (s & 2u) {
    gc_exi_probeex_ret[1] = 0; // busy
  }
  if (s & 4u) {
    gc_exi_probeex_ret[2] = 2; // present
    gc_exi_getid_ok[2] = 1;
    gc_exi_id[2] = 0x12345678u ^ s;
  }
}

static u32 h_one(s32 chan) {
  u32 id = 0xDEADBEEFu;
  s32 pe = oracle_EXIProbeEx(chan);
  u32 p = oracle_EXIProbe(chan);
  s32 ok = oracle_EXIGetID(chan, 0, &id);
  u32 x = 0;
  x = rotl1(x) ^ (u32)pe;
  x = rotl1(x) ^ p;
  x = rotl1(x) ^ (u32)ok;
  x = rotl1(x) ^ id;
  return x;
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);

  volatile uint8_t *out = (volatile uint8_t *)0x80300000u;
  u32 off = 0;
  u32 tc = 0;
  u32 m = 0;

  // L0: fixed cases.
  u32 a = 0;
  reset_cfg(0); a = rotl1(a) ^ h_one(0);
  reset_cfg(1); a = rotl1(a) ^ h_one(0);
  reset_cfg(2); a = rotl1(a) ^ h_one(1);
  reset_cfg(4); a = rotl1(a) ^ h_one(2);
  reset_cfg(7); a = rotl1(a) ^ h_one(2);
  tc += 5; m = rotl1(m) ^ a;

  // L1: accumulation (vary cfg seed).
  u32 b = 0;
  for (u32 i = 0; i < 32; i++) {
    reset_cfg(0x100u + i);
    b = rotl1(b) ^ h_one((s32)(i % 3u));
  }
  tc += 32; m = rotl1(m) ^ b;

  // L2: overwrite/idempotency (flip a channel between absent/busy/present).
  u32 c = 0;
  reset_cfg(0);
  for (u32 i = 0; i < 16; i++) {
    gc_exi_probeex_ret[0] = (i & 1u) ? 1 : -1;
    gc_exi_getid_ok[0] = (i & 1u) ? 1 : 0;
    gc_exi_id[0] = 0xABCD0000u ^ i;
    c = rotl1(c) ^ h_one(0);
  }
  tc += 16; m = rotl1(m) ^ c;

  // L3: random-start deterministic.
  u32 d = 0;
  rs = 0x5EED1234u;
  for (u32 i = 0; i < 256; i++) {
    reset_cfg(rn());
    int chan = (int)(rn() % 3u);
    d = rotl1(d) ^ h_one(chan);
  }
  tc += 256; m = rotl1(m) ^ d;

  // L4: simple "game-like" busy -> present transition.
  u32 e = 0;
  reset_cfg(0);
  gc_exi_probeex_ret[0] = 0; // busy
  for (u32 i = 0; i < 3; i++) e = rotl1(e) ^ h_one(0);
  gc_exi_probeex_ret[0] = 1; // present
  gc_exi_getid_ok[0] = 1;
  gc_exi_id[0] = 0x00000004u;
  e = rotl1(e) ^ h_one(0);
  tc += 4; m = rotl1(m) ^ e;

  // L5: boundary channels.
  u32 f = 0;
  reset_cfg(1);
  f = rotl1(f) ^ h_one(-1);
  f = rotl1(f) ^ h_one(3);
  tc += 2; m = rotl1(m) ^ f;

  be(out + off * 4u, 0x45584950u); off++; // "EXIP"
  be(out + off * 4u, tc); off++;
  be(out + off * 4u, m); off++;
  be(out + off * 4u, a); off++;
  be(out + off * 4u, b); off++;
  be(out + off * 4u, c); off++;
  be(out + off * 4u, d); off++;
  be(out + off * 4u, e); off++;
  be(out + off * 4u, f); off++;

  // A few direct samples for debugging diffs.
  reset_cfg(0); be(out + off * 4u, (u32)oracle_EXIProbeEx(0)); off++;
  reset_cfg(1); { u32 id=0; oracle_EXIGetID(0,0,&id); be(out+off*4u,id); off++; }

  while (1) { __asm__ volatile("nop"); }
  return 0;
}

