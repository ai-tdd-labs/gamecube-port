#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

extern void oracle_EXIInit(void);
extern u32 oracle_exi_select_ok[3];
extern u32 oracle_exi_card_status[3];
extern u32 oracle_exi_card_status_reads[3];
extern u32 oracle_exi_card_status_clears[3];
extern u32 oracle_exi_last_imm_len[3];
extern u32 oracle_exi_last_imm_type[3];
extern u32 oracle_exi_last_imm_data[3];

int EXILock(s32 channel, u32 device, void* cb);
int EXIUnlock(s32 channel);

s32 oracle___CARDReadStatus(s32 chan, u8* status);
s32 oracle___CARDClearStatus(s32 chan);

static inline void wr32be(volatile u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline u32 h32(u32 h, u32 v) { return rotl1(h) ^ v; }

int main(void) {
  volatile u8* out = (volatile u8*)0x80300000u;
  for (u32 i = 0; i < 0x40u; i++) out[i] = 0;

  u32 l0 = 0, l1 = 0, l2 = 0, l5 = 0;

  // L0: without lock, EXISelect fails -> NOCARD.
  oracle_EXIInit();
  oracle_exi_select_ok[0] = 1;
  u8 st = 0xEE;
  l0 = h32(l0, (u32)oracle___CARDReadStatus(0, &st));
  l0 = h32(l0, (u32)st);
  l0 = h32(l0, (u32)oracle___CARDClearStatus(0));

  // L1: with lock, status reads and clears.
  oracle_EXIInit();
  (void)EXILock(0, 0, 0);
  oracle_exi_card_status[0] = 0x40u;
  st = 0xEE;
  l1 = h32(l1, (u32)oracle___CARDReadStatus(0, &st));
  l1 = h32(l1, (u32)st);
  l1 = h32(l1, oracle_exi_card_status_reads[0]);
  l1 = h32(l1, oracle_exi_last_imm_len[0]);
  l1 = h32(l1, oracle_exi_last_imm_type[0]);
  l1 = h32(l1, oracle_exi_last_imm_data[0]); // should reflect status byte on read

  l1 = h32(l1, (u32)oracle___CARDClearStatus(0));
  l1 = h32(l1, oracle_exi_card_status_clears[0]);
  l1 = h32(l1, oracle_exi_last_imm_data[0]); // should reflect 0x89000000 on write

  st = 0xEE;
  l1 = h32(l1, (u32)oracle___CARDReadStatus(0, &st));
  l1 = h32(l1, (u32)st);
  (void)EXIUnlock(0);

  // L2: EXISelect forced-fail -> NOCARD even when locked.
  oracle_EXIInit();
  (void)EXILock(0, 0, 0);
  oracle_exi_select_ok[0] = 0;
  st = 0xEE;
  l2 = h32(l2, (u32)oracle___CARDReadStatus(0, &st));
  l2 = h32(l2, (u32)st);
  l2 = h32(l2, (u32)oracle___CARDClearStatus(0));
  (void)EXIUnlock(0);

  // L5: channel out of range -> NOCARD (via EXISelect bounds).
  oracle_EXIInit();
  st = 0xEE;
  l5 = h32(l5, (u32)oracle___CARDReadStatus(-1, &st));
  l5 = h32(l5, (u32)oracle___CARDReadStatus(3, &st));
  l5 = h32(l5, (u32)oracle___CARDClearStatus(-1));
  l5 = h32(l5, (u32)oracle___CARDClearStatus(3));

  u32 master = 0;
  master = h32(master, l0);
  master = h32(master, l1);
  master = h32(master, l2);
  master = h32(master, l5);

  wr32be(out + 0x00, 0x43535453u); // "CSTS"
  wr32be(out + 0x04, master);
  wr32be(out + 0x08, l0);
  wr32be(out + 0x0C, l1);
  wr32be(out + 0x10, l2);
  wr32be(out + 0x14, l5);
  // Small raw debug: final read/clear counters for chan0
  wr32be(out + 0x18, oracle_exi_card_status_reads[0]);
  wr32be(out + 0x1C, oracle_exi_card_status_clears[0]);

  while (1) {
  }
  return 0;
}

