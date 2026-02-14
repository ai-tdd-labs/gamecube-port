#include "gc_host_scenario.h"
#include "gc_mem.h"

#include "dolphin/exi.h"

#include <stdint.h>

typedef uint32_t u32;
typedef int32_t s32;

extern u32 gc_exi_regs[16];

enum { L0 = 8, L1 = 32, L2 = 16, L3 = 2048, L4 = 6, L5 = 4 };

static inline void be_store(u32 addr, u32 v) {
  uint8_t* p = gc_mem_ptr(addr, 4);
  if (!p) return;
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline u32 h_state(s32 chan) {
  u32 h = 0;
  h = rotl1(h) ^ EXIGetState(chan);
  u32 base = (u32)chan * 5u;
  h = rotl1(h) ^ gc_exi_regs[base + 0];
  h = rotl1(h) ^ gc_exi_regs[base + 3];
  h = rotl1(h) ^ gc_exi_regs[base + 4];
  return h;
}

static u32 rs;
static inline u32 rn(void) {
  rs ^= rs << 13;
  rs ^= rs >> 17;
  rs ^= rs << 5;
  return rs;
}

static inline void seed_imm(u32 v) { gc_exi_regs[4] = v; }

static inline void run_chain_once(s32 chan, u32 dev, u32 freq, u32 imm_seed, u32 rw, u32 len) {
  uint8_t buf[4] = {0, 0, 0, 0};
  if (rw == EXI_WRITE) {
    buf[0] = (uint8_t)(imm_seed >> 24);
    buf[1] = (uint8_t)(imm_seed >> 16);
    buf[2] = (uint8_t)(imm_seed >> 8);
    buf[3] = (uint8_t)(imm_seed >> 0);
  } else {
    seed_imm(imm_seed);
  }

  (void)EXILock(chan, dev, 0);
  (void)EXISelect(chan, dev, freq);
  (void)EXIImm(chan, buf, (s32)len, rw, 0);
  (void)EXISync(chan);
  (void)EXIDeselect(chan);
  (void)EXIUnlock(chan);

  if (rw == EXI_READ) {
    u32 packed = ((u32)buf[0] << 24) | ((u32)buf[1] << 16) | ((u32)buf[2] << 8) | (u32)buf[3];
    gc_exi_regs[4] = packed;
  }
}

const char* gc_scenario_label(void) { return "EXI/exi_min_chain_pbt_001"; }

const char* gc_scenario_out_path(void) { return "../actual/exi_min_chain_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
  (void)ram;

  EXIInit();

  u32 out = 0x80300000u;
  u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

  // L0
  {
    const u32 lens[8] = {1, 2, 3, 4, 4, 3, 2, 1};
    const u32 rws[8] = {EXI_WRITE, EXI_WRITE, EXI_WRITE, EXI_WRITE, EXI_READ, EXI_READ, EXI_READ, EXI_READ};
    for (u32 i = 0; i < 8; i++) {
      EXIInit();
      run_chain_once(0, 1u, 3u, 0xA1B2C3D4u ^ (i * 0x01010101u), rws[i], lens[i]);
      a = rotl1(a) ^ h_state(0);
    }
    tc += L0;
    m = rotl1(m) ^ a;
  }

  // L1
  for (u32 i = 0; i < L1; i++) {
    EXIInit();
    for (u32 j = 0; j < 4; j++) {
      run_chain_once(0, 1u, 3u, 0x01020304u ^ (i * 0x11111111u) ^ j, (j & 1) ? EXI_READ : EXI_WRITE,
                     (j % 4) + 1);
    }
    b = rotl1(b) ^ h_state(0);
  }
  tc += L1;
  m = rotl1(m) ^ b;

  // L2
  for (u32 i = 0; i < L2; i++) {
    EXIInit();
    uint8_t buf[4] = {1, 2, 3, 4};
    (void)EXILock(0, 1u, 0);
    (void)EXILock(0, 1u, 0);
    (void)EXISelect(0, 1u, 3u);
    (void)EXISelect(0, 1u, 3u);
    (void)EXIImm(0, buf, 4, EXI_WRITE, 0);
    (void)EXISync(0);
    (void)EXIDeselect(0);
    (void)EXIUnlock(0);
    (void)EXIUnlock(0);
    c = rotl1(c) ^ h_state(0);
  }
  tc += L2;
  m = rotl1(m) ^ c;

  // L3
  rs = 0x13579BDFu;
  for (u32 i = 0; i < L3; i++) {
    EXIInit();
    u32 rw = (rn() & 1u) ? EXI_READ : EXI_WRITE;
    u32 len = (rn() & 3u) + 1u;
    run_chain_once(0, (rn() & 3u), (rn() & 7u), rn(), rw, len);
    d = rotl1(d) ^ h_state(0);
  }
  tc += L3;
  m = rotl1(m) ^ d;

  // L4
  EXIInit();
  {
    uint8_t cmd2[2] = {0x20, 0x00};
    (void)EXILock(0, 1u, 0);
    (void)EXISelect(0, 1u, 3u);
    (void)EXIImm(0, cmd2, 2, EXI_WRITE, 0);
    (void)EXISync(0);
    seed_imm(0x11223344u);
    uint8_t out4[4] = {0, 0, 0, 0};
    (void)EXIImm(0, out4, 4, EXI_READ, 0);
    (void)EXISync(0);
    (void)EXIDeselect(0);
    (void)EXIUnlock(0);
    gc_exi_regs[4] = ((u32)out4[0] << 24) | ((u32)out4[1] << 16) | ((u32)out4[2] << 8) | (u32)out4[3];
    e = rotl1(e) ^ h_state(0);
  }
  (void)EXISync(0);
  (void)EXISync(0);
  e = rotl1(e) ^ h_state(0);
  tc += L4;
  m = rotl1(m) ^ e;

  // L5
  for (u32 i = 0; i < L5; i++) {
    EXIInit();
    uint8_t b4[4] = {0, 0, 0, 0};
    (void)EXILock(0, 1u, 0);
    (void)EXISelect(0, 1u, 3u);
    (void)EXIImm(0, b4, 0, EXI_WRITE, 0);
    (void)EXIImm(0, b4, 5, EXI_WRITE, 0);
    (void)EXIDeselect(0);
    (void)EXIUnlock(0);
    f = rotl1(f) ^ h_state(0);
  }
  tc += L5;
  m = rotl1(m) ^ f;

  be_store(out + off * 4u, 0x45584950u); off++; // EXIP
  be_store(out + off * 4u, tc); off++;
  be_store(out + off * 4u, m); off++;
  be_store(out + off * 4u, a); off++; be_store(out + off * 4u, L0); off++;
  be_store(out + off * 4u, b); off++; be_store(out + off * 4u, L1); off++;
  be_store(out + off * 4u, c); off++; be_store(out + off * 4u, L2); off++;
  be_store(out + off * 4u, d); off++; be_store(out + off * 4u, L3); off++;
  be_store(out + off * 4u, e); off++; be_store(out + off * 4u, L4); off++;
  be_store(out + off * 4u, f); off++; be_store(out + off * 4u, L5); off++;

  EXIInit();
  run_chain_once(0, 1u, 3u, 0xCAFEBABEu, EXI_READ, 4);
  be_store(out + off * 4u, EXIGetState(0)); off++;
  be_store(out + off * 4u, gc_exi_regs[0]); off++;
  be_store(out + off * 4u, gc_exi_regs[3]); off++;
  be_store(out + off * 4u, gc_exi_regs[4]); off++;
}
