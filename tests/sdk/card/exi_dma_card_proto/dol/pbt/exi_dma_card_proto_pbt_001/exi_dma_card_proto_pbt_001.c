#include <stdint.h>
#include <string.h>

#include "src/sdk_port/gc_mem.c"

typedef uint32_t u32;
typedef int32_t s32;

enum { EXI_READ = 0, EXI_WRITE = 1 };

void oracle_EXIInit(void);
int oracle_EXILock(s32 chan, u32 dev, void* cb);
int oracle_EXIUnlock(s32 chan);
int oracle_EXISelect(s32 chan, u32 dev, u32 freq);
int oracle_EXIDeselect(s32 chan);
int oracle_EXIImmEx(s32 chan, void* buf, s32 len, u32 type);
int oracle_EXIDma(s32 chan, void* buffer, s32 length, u32 type, void* cb);
int oracle_card_peek(u32 addr, void* dst, u32 len);

static inline void be(volatile uint8_t* p, u32 v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static u32 fnv1a32(const uint8_t* p, u32 n) {
  u32 h = 2166136261u;
  for (u32 i = 0; i < n; i++) {
    h ^= (u32)p[i];
    h *= 16777619u;
  }
  return h;
}

static void encode_cmd(uint8_t* cmd5, uint8_t op, u32 addr) {
  cmd5[0] = op;
  cmd5[1] = (uint8_t)((addr >> 17) & 0x7Fu);
  cmd5[2] = (uint8_t)((addr >> 9) & 0xFFu);
  cmd5[3] = (uint8_t)((addr >> 7) & 0x03u);
  cmd5[4] = (uint8_t)(addr & 0x7Fu);
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (uint8_t*)0x80000000u);

  volatile uint8_t* out = (volatile uint8_t*)0x80300000u;
  u32 off = 0;

  oracle_EXIInit();
  oracle_EXILock(0, 0, 0);
  oracle_EXISelect(0, 0, 4);

  // Read 512 bytes @0x21000 (exercises cmd1 nonzero).
  uint8_t cmd[5];
  encode_cmd(cmd, 0x52, 0x21000u);
  oracle_EXIImmEx(0, cmd, 5, EXI_WRITE);

  uint8_t buf[512];
  memset(buf, 0, sizeof(buf));
  int rd_ok = oracle_EXIDma(0, buf, (s32)sizeof(buf), EXI_READ, 0);
  u32 rd_h = fnv1a32(buf, (u32)sizeof(buf));

  // Write 128 bytes @0x22000 then read-back from oracle store.
  encode_cmd(cmd, 0xF2, 0x22000u);
  oracle_EXIImmEx(0, cmd, 5, EXI_WRITE);
  uint8_t wr[128];
  for (u32 i = 0; i < (u32)sizeof(wr); i++) wr[i] = (uint8_t)(0x5Au ^ (uint8_t)i);
  int wr_ok = oracle_EXIDma(0, wr, (s32)sizeof(wr), EXI_WRITE, 0);
  uint8_t back[128];
  memset(back, 0, sizeof(back));
  int bk_ok = oracle_card_peek(0x22000u, back, (u32)sizeof(back));
  u32 bk_h = fnv1a32(back, (u32)sizeof(back));

  oracle_EXIDeselect(0);
  oracle_EXIUnlock(0);

  be(out + off * 4u, 0x5844494Du); off++; // "XDIM" (EXI DMA)
  be(out + off * 4u, (u32)rd_ok); off++;
  be(out + off * 4u, rd_h); off++;
  be(out + off * 4u, (u32)wr_ok); off++;
  be(out + off * 4u, (u32)bk_ok); off++;
  be(out + off * 4u, bk_h); off++;

  while (1) { __asm__ volatile("nop"); }
  return 0;
}
