#include <stdint.h>
#include <string.h>

#include "src/sdk_port/gc_mem.c"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

extern void oracle_EXIInit(void);
extern u32 oracle_exi_select_ok[3];
extern u32 oracle_exi_last_imm_len[3];
extern u32 oracle_exi_last_imm_type[3];
extern u32 oracle_exi_last_imm_data[3];

int EXILock(s32 channel, u32 device, void* cb);
int EXIUnlock(s32 channel);

s32 oracle___CARDEnableInterrupt(s32 chan, BOOL enable);

static inline void wr32be(volatile u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline u32 h32(u32 h, u32 v) { return rotl1(h) ^ v; }

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (u8*)0x80000000u);
  volatile u8* out = (volatile u8*)0x80300000u;
  for (u32 i = 0; i < 0x40u; i++) out[i] = 0;

  u32 l0 = 0, l1 = 0;

  // L0: basic enable/disable command bytes.
  oracle_EXIInit();
  (void)EXILock(0, 0, 0);
  l0 = h32(l0, (u32)oracle___CARDEnableInterrupt(0, 1));
  l0 = h32(l0, oracle_exi_last_imm_len[0]);
  l0 = h32(l0, oracle_exi_last_imm_type[0]);
  l0 = h32(l0, oracle_exi_last_imm_data[0]);

  l0 = h32(l0, (u32)oracle___CARDEnableInterrupt(0, 0));
  l0 = h32(l0, oracle_exi_last_imm_data[0]);
  (void)EXIUnlock(0);

  // L1: select failure -> NOCARD.
  oracle_EXIInit();
  (void)EXILock(0, 0, 0);
  oracle_exi_select_ok[0] = 0;
  l1 = h32(l1, (u32)oracle___CARDEnableInterrupt(0, 1));
  (void)EXIUnlock(0);

  u32 master = 0;
  master = h32(master, l0);
  master = h32(master, l1);

  wr32be(out + 0x00, 0x4345494Eu); // "CEIN"
  wr32be(out + 0x04, master);
  wr32be(out + 0x08, l0);
  wr32be(out + 0x0C, l1);

  while (1) {}
  return 0;
}
