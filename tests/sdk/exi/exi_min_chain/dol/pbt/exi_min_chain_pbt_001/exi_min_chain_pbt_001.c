#include <stdint.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

enum { FALSE = 0, TRUE = 1 };

// Oracle implementations are provided in oracle_exi_min_chain.c.
void oracle_EXIReset(void);
BOOL EXILock(s32 channel, u32 device, void* cb);
BOOL EXIUnlock(s32 channel);
BOOL EXISelect(s32 channel, u32 device, u32 freq);
BOOL EXIDeselect(s32 channel);
BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, void* cb);
BOOL EXISync(s32 channel);
u32 EXIGetState(s32 channel);
extern u32 gc_exi_regs[16];

enum { EXI_READ = 0, EXI_WRITE = 1 };

enum { L0 = 8, L1 = 32, L2 = 16, L3 = 2048, L4 = 6, L5 = 4 };

static inline void be(volatile uint8_t* p, u32 v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static inline u32 h_state(s32 chan) {
  u32 h = 0;
  h = rotl1(h) ^ EXIGetState(chan);
  // Hash a few regs: STAT, CONTROL, IMM for chan.
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

static inline void seed_imm(u32 v) {
  // Put bytes in IMM (MSB-first).
  gc_exi_regs[4] = v;
}

static inline void run_chain_once(s32 chan, u32 dev, u32 freq, u32 imm_seed, u32 rw, u32 len) {
  uint8_t buf[4] = {0, 0, 0, 0};
  if (rw == EXI_WRITE) {
    buf[0] = (uint8_t)(imm_seed >> 24);
    buf[1] = (uint8_t)(imm_seed >> 16);
    buf[2] = (uint8_t)(imm_seed >> 8);
    buf[3] = (uint8_t)(imm_seed >> 0);
  } else {
    // For reads, seed the IMM register to a known value that Sync will unpack into buf.
    seed_imm(imm_seed);
  }

  (void)EXILock(chan, dev, 0);
  (void)EXISelect(chan, dev, freq);
  (void)EXIImm(chan, buf, (s32)len, rw, 0);
  (void)EXISync(chan);
  (void)EXIDeselect(chan);
  (void)EXIUnlock(chan);

  // For read cases, fold buf back into IMM for hashing stability (observable).
  if (rw == EXI_READ) {
    u32 packed = ((u32)buf[0] << 24) | ((u32)buf[1] << 16) | ((u32)buf[2] << 8) | (u32)buf[3];
    gc_exi_regs[4] = packed;
  }
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t*)0x80000000u);

  volatile uint8_t* out = (volatile uint8_t*)0x80300000u;
  u32 off = 0, tc = 0, m = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;

  // L0: fixed edge combos.
  {
    const u32 lens[8] = {1, 2, 3, 4, 4, 3, 2, 1};
    const u32 rws[8] = {EXI_WRITE, EXI_WRITE, EXI_WRITE, EXI_WRITE, EXI_READ, EXI_READ, EXI_READ, EXI_READ};
    for (u32 i = 0; i < 8; i++) {
      oracle_EXIReset();
      run_chain_once(0, 1u, 3u, 0xA1B2C3D4u ^ (i * 0x01010101u), rws[i], lens[i]);
      a = rotl1(a) ^ h_state(0);
    }
    tc += L0; m = rotl1(m) ^ a;
  }

  // L1: accumulation (many sequential chains).
  for (u32 i = 0; i < L1; i++) {
    oracle_EXIReset();
    for (u32 j = 0; j < 4; j++) {
      run_chain_once(0, 1u, 3u, 0x01020304u ^ (i * 0x11111111u) ^ j, (j & 1) ? EXI_READ : EXI_WRITE, (j % 4) + 1);
    }
    b = rotl1(b) ^ h_state(0);
  }
  tc += L1; m = rotl1(m) ^ b;

  // L2: idempotency transitions (double select/lock attempts).
  for (u32 i = 0; i < L2; i++) {
    oracle_EXIReset();
    uint8_t buf[4] = {1, 2, 3, 4};
    (void)EXILock(0, 1u, 0);
    (void)EXILock(0, 1u, 0); // should fail but state stable
    (void)EXISelect(0, 1u, 3u);
    (void)EXISelect(0, 1u, 3u); // should fail
    (void)EXIImm(0, buf, 4, EXI_WRITE, 0);
    (void)EXISync(0);
    (void)EXIDeselect(0);
    (void)EXIUnlock(0);
    (void)EXIUnlock(0); // second unlock
    c = rotl1(c) ^ h_state(0);
  }
  tc += L2; m = rotl1(m) ^ c;

  // L3: random-start chains.
  rs = 0x13579BDFu;
  for (u32 i = 0; i < L3; i++) {
    oracle_EXIReset();
    u32 rw = (rn() & 1u) ? EXI_READ : EXI_WRITE;
    u32 len = (rn() & 3u) + 1u;
    run_chain_once(0, (rn() & 3u), (rn() & 7u), rn(), rw, len);
    d = rotl1(d) ^ h_state(0);
  }
  tc += L3; m = rotl1(m) ^ d;

  // L4: callsite-ish (RTC style: chan0 dev1 freq3, write 2 bytes then read 4).
  oracle_EXIReset();
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
    // Fold out4 into IMM for hash.
    gc_exi_regs[4] = ((u32)out4[0] << 24) | ((u32)out4[1] << 16) | ((u32)out4[2] << 8) | (u32)out4[3];
    e = rotl1(e) ^ h_state(0);
  }
  // add some no-op syncs.
  (void)EXISync(0);
  (void)EXISync(0);
  e = rotl1(e) ^ h_state(0);
  tc += L4; m = rotl1(m) ^ e;

  // L5: boundary around invalid lengths.
  for (u32 i = 0; i < L5; i++) {
    oracle_EXIReset();
    uint8_t b4[4] = {0, 0, 0, 0};
    (void)EXILock(0, 1u, 0);
    (void)EXISelect(0, 1u, 3u);
    (void)EXIImm(0, b4, 0, EXI_WRITE, 0);
    (void)EXIImm(0, b4, 5, EXI_WRITE, 0);
    (void)EXIDeselect(0);
    (void)EXIUnlock(0);
    f = rotl1(f) ^ h_state(0);
  }
  tc += L5; m = rotl1(m) ^ f;

  be(out + off * 4u, 0x45584950u); off++; // EXIP
  be(out + off * 4u, tc); off++;
  be(out + off * 4u, m); off++;
  be(out + off * 4u, a); off++; be(out + off * 4u, L0); off++;
  be(out + off * 4u, b); off++; be(out + off * 4u, L1); off++;
  be(out + off * 4u, c); off++; be(out + off * 4u, L2); off++;
  be(out + off * 4u, d); off++; be(out + off * 4u, L3); off++;
  be(out + off * 4u, e); off++; be(out + off * 4u, L4); off++;
  be(out + off * 4u, f); off++; be(out + off * 4u, L5); off++;

  // Probe dump: one chain and dump state+regs.
  oracle_EXIReset();
  run_chain_once(0, 1u, 3u, 0xCAFEBABEu, EXI_READ, 4);
  be(out + off * 4u, EXIGetState(0)); off++;
  be(out + off * 4u, gc_exi_regs[0]); off++;
  be(out + off * 4u, gc_exi_regs[3]); off++;
  be(out + off * 4u, gc_exi_regs[4]); off++;

  while (1) {
    __asm__ volatile("nop");
  }
  return 0;
}

