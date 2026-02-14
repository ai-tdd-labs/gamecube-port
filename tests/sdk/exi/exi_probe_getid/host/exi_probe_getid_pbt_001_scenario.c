#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/types.h"
#include "dolphin/exi.h"

static inline void be(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static inline uint32_t rotl1(uint32_t v) { return (v << 1) | (v >> 31); }

static uint32_t rs;
static inline uint32_t rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static void reset_cfg(uint32_t s) {
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
    gc_exi_probeex_ret[1] = 0;
  }
  if (s & 4u) {
    gc_exi_probeex_ret[2] = 2;
    gc_exi_getid_ok[2] = 1;
    gc_exi_id[2] = 0x12345678u ^ s;
  }
}

static uint32_t h_one(int chan) {
  uint32_t id = 0xDEADBEEFu;
  int32_t pe = EXIProbeEx(chan);
  uint32_t p = (uint32_t)EXIProbe(chan);
  int32_t ok = EXIGetID(chan, 0, &id);
  uint32_t x = 0;
  x = rotl1(x) ^ (uint32_t)pe;
  x = rotl1(x) ^ p;
  x = rotl1(x) ^ (uint32_t)ok;
  x = rotl1(x) ^ id;
  return x;
}

const char *gc_scenario_label(void) { return "exi_probe_getid_pbt_001"; }

const char *gc_scenario_out_path(void) {
  return "../actual/exi_probe_getid_pbt_001.bin";
}

void gc_scenario_run(GcRam *ram) {
  uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x80);
  uint32_t off = 0;
  uint32_t tc = 0;
  uint32_t m = 0;

  uint32_t a = 0;
  reset_cfg(0); a = rotl1(a) ^ h_one(0);
  reset_cfg(1); a = rotl1(a) ^ h_one(0);
  reset_cfg(2); a = rotl1(a) ^ h_one(1);
  reset_cfg(4); a = rotl1(a) ^ h_one(2);
  reset_cfg(7); a = rotl1(a) ^ h_one(2);
  tc += 5; m = rotl1(m) ^ a;

  uint32_t b = 0;
  for (uint32_t i = 0; i < 32; i++) {
    reset_cfg(0x100u + i);
    b = rotl1(b) ^ h_one((int)(i % 3u));
  }
  tc += 32; m = rotl1(m) ^ b;

  uint32_t c = 0;
  reset_cfg(0);
  for (uint32_t i = 0; i < 16; i++) {
    gc_exi_probeex_ret[0] = (i & 1u) ? 1 : -1;
    gc_exi_getid_ok[0] = (i & 1u) ? 1 : 0;
    gc_exi_id[0] = 0xABCD0000u ^ i;
    c = rotl1(c) ^ h_one(0);
  }
  tc += 16; m = rotl1(m) ^ c;

  uint32_t d = 0;
  rs = 0x5EED1234u;
  for (uint32_t i = 0; i < 256; i++) {
    reset_cfg(rn());
    int chan = (int)(rn() % 3u);
    d = rotl1(d) ^ h_one(chan);
  }
  tc += 256; m = rotl1(m) ^ d;

  uint32_t e = 0;
  reset_cfg(0);
  gc_exi_probeex_ret[0] = 0;
  for (uint32_t i = 0; i < 3; i++) e = rotl1(e) ^ h_one(0);
  gc_exi_probeex_ret[0] = 1;
  gc_exi_getid_ok[0] = 1;
  gc_exi_id[0] = 0x00000004u;
  e = rotl1(e) ^ h_one(0);
  tc += 4; m = rotl1(m) ^ e;

  uint32_t f = 0;
  reset_cfg(1);
  f = rotl1(f) ^ h_one(-1);
  f = rotl1(f) ^ h_one(3);
  tc += 2; m = rotl1(m) ^ f;

  be(out + off * 4u, 0x45584950u); off++;
  be(out + off * 4u, tc); off++;
  be(out + off * 4u, m); off++;
  be(out + off * 4u, a); off++;
  be(out + off * 4u, b); off++;
  be(out + off * 4u, c); off++;
  be(out + off * 4u, d); off++;
  be(out + off * 4u, e); off++;
  be(out + off * 4u, f); off++;

  reset_cfg(0); be(out + off * 4u, (uint32_t)EXIProbeEx(0)); off++;
  reset_cfg(1); { uint32_t id=0; EXIGetID(0,0,&id); be(out+off*4u,id); off++; }
}
