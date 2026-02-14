#include <stdint.h>
#include <string.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef int32_t s32;
typedef uint8_t u8;

extern s32 gc_exi_probeex_ret[3];
extern u32 gc_exi_getid_ok[3];
extern u32 gc_exi_id[3];
extern u32 gc_exi_state[3];

typedef struct {
  s32 attached;
  s32 mount_step;
  s32 size_mb;
  s32 sector_size;
} GcCardControl;
extern GcCardControl gc_card_block[2];

s32 oracle_CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize);

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

static void reset_state(u32 s) {
  // Clear GameChoice by default.
  *(volatile u8*)0x800030E3u = 0;

  for (int i = 0; i < 3; i++) {
    gc_exi_probeex_ret[i] = -1;
    gc_exi_getid_ok[i] = 0;
    gc_exi_id[i] = 0;
    gc_exi_state[i] = 0;
  }
  for (int c = 0; c < 2; c++) {
    gc_card_block[c].attached = 0;
    gc_card_block[c].mount_step = 0;
    gc_card_block[c].size_mb = 0;
    gc_card_block[c].sector_size = 0;
  }

  // Seed a few defaults based on s for L3 randomness.
  gc_exi_probeex_ret[0] = (s & 1u) ? 1 : -1;
  gc_exi_probeex_ret[1] = (s & 2u) ? 0 : -1;
  gc_exi_getid_ok[0] = (s & 4u) ? 1u : 0u;
  gc_exi_id[0] = 0x00000004u ^ s; // valid "card-like" id when upper bits are 0
}

static u32 one(s32 chan) {
  s32 mem = (s32)0xAAAAAAAAu;
  s32 sec = (s32)0xBBBBBBBBu;
  s32 r = oracle_CARDProbeEx(chan, &mem, &sec);
  u32 h = 2166136261u;
  h = fnv1a32_u32(h, (u32)r);
  h = fnv1a32_u32(h, (u32)mem);
  h = fnv1a32_u32(h, (u32)sec);
  return h;
}

static u32 rs;
static inline u32 rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (u8*)0x80000000u);

  volatile u8* out = (volatile u8*)0x80300000u;
  u32 off = 0;
  u32 tc = 0;
  u32 m = 0;

  // L0: fixed matrix of key branches.
  u32 a = 0;
  reset_state(0);
  a = rotl1(a) ^ one(-1);
  a = rotl1(a) ^ one(2);

  reset_state(0); gc_exi_probeex_ret[0] = -1; a = rotl1(a) ^ one(0);
  reset_state(0); gc_exi_probeex_ret[0] = 0; a = rotl1(a) ^ one(0);
  reset_state(0); gc_exi_probeex_ret[0] = 1; gc_exi_state[0] = 8; a = rotl1(a) ^ one(0); // wrong device
  reset_state(0); gc_exi_probeex_ret[0] = 1; gc_exi_getid_ok[0] = 0; a = rotl1(a) ^ one(0); // GetID busy
  reset_state(0); gc_exi_probeex_ret[0] = 1; gc_exi_getid_ok[0] = 1; gc_exi_id[0] = 0x00000004u; a = rotl1(a) ^ one(0); // valid card id
  reset_state(0); gc_exi_probeex_ret[0] = 1; gc_exi_getid_ok[0] = 1; gc_exi_id[0] = 0x00000005u; a = rotl1(a) ^ one(0); // invalid card id -> wrongdevice

  reset_state(0); gc_exi_probeex_ret[0] = 1; gc_card_block[0].attached = 1; gc_card_block[0].mount_step = 0; a = rotl1(a) ^ one(0);
  reset_state(0); gc_exi_probeex_ret[0] = 1; gc_card_block[0].attached = 1; gc_card_block[0].mount_step = 1; gc_card_block[0].size_mb = 64; gc_card_block[0].sector_size = 8192; a = rotl1(a) ^ one(0);

  reset_state(0); *(volatile u8*)0x800030E3u = 0x80; a = rotl1(a) ^ one(0);

  tc += 11; m = rotl1(m) ^ a;

  // L1: accumulation (vary probe/GetID config).
  u32 b = 0;
  for (u32 i = 0; i < 64; i++) {
    reset_state(0x100u + i);
    gc_exi_probeex_ret[0] = (i & 3u) == 0 ? -1 : ((i & 3u) == 1 ? 0 : 1);
    gc_exi_getid_ok[0] = (i & 1u);
    gc_exi_id[0] = 0x00000004u ^ (i << 11);
    b = rotl1(b) ^ one(0);
  }
  tc += 64; m = rotl1(m) ^ b;

  // L2: overwrite transitions on attached/mount_step.
  u32 c = 0;
  reset_state(0);
  gc_exi_probeex_ret[0] = 1;
  gc_card_block[0].attached = 1;
  for (u32 i = 0; i < 16; i++) {
    gc_card_block[0].mount_step = (i & 1u) ? 1 : 0;
    gc_card_block[0].size_mb = (s32)(4 + (i & 0x7u));
    gc_card_block[0].sector_size = (s32)((i & 1u) ? 8192 : 16384);
    c = rotl1(c) ^ one(0);
  }
  tc += 16; m = rotl1(m) ^ c;

  // L3: random-start deterministic.
  u32 d = 0;
  rs = 0xBADC0FFEu;
  for (u32 i = 0; i < 256; i++) {
    reset_state(rn());
    int chan = (int)(rn() % 2u);
    // Randomly choose attached vs not and id ok.
    if (rn() & 1u) {
      gc_exi_probeex_ret[chan] = 1;
      gc_card_block[chan].attached = 1;
      gc_card_block[chan].mount_step = (rn() & 1u) ? 1 : 0;
      gc_card_block[chan].size_mb = (s32)(rn() & 0xFCu);
      gc_card_block[chan].sector_size = (rn() & 1u) ? 8192 : 32768;
    } else {
      gc_exi_probeex_ret[chan] = (rn() & 1u) ? 1 : -1;
      gc_exi_getid_ok[chan] = (rn() & 1u) ? 1 : 0;
      gc_exi_id[chan] = (rn() & 1u) ? 0x00000004u : 0x00000005u;
    }
    d = rotl1(d) ^ one(chan);
  }
  tc += 256; m = rotl1(m) ^ d;

  // L4: callsite-style busy spin then ready.
  u32 e = 0;
  reset_state(0);
  gc_exi_probeex_ret[0] = 0; // busy
  for (u32 i = 0; i < 3; i++) e = rotl1(e) ^ one(0);
  gc_exi_probeex_ret[0] = 1;
  gc_exi_getid_ok[0] = 1;
  gc_exi_id[0] = 0x00000004u;
  e = rotl1(e) ^ one(0);
  tc += 4; m = rotl1(m) ^ e;

  // L5: boundary (null outputs).
  u32 f = 0;
  reset_state(0);
  gc_exi_probeex_ret[0] = 1;
  gc_exi_getid_ok[0] = 1;
  gc_exi_id[0] = 0x00000004u;
  s32 r = oracle_CARDProbeEx(0, 0, 0);
  u32 h = 2166136261u;
  h = fnv1a32_u32(h, (u32)r);
  f = rotl1(f) ^ h;
  tc += 1; m = rotl1(m) ^ f;

  be(out + off * 4u, 0x43505242u); off++; // "CPRB"
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

