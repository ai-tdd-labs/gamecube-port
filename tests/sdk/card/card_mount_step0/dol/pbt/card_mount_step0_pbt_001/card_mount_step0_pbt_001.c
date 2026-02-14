#include <stdint.h>
#include <string.h>

#include "src/sdk_port/gc_mem.c"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;

extern void oracle_reset(void);
extern s32 oracle_CARDMountAsync_step0(s32 chan);

extern s32 oracle_exi_probeex_ret[3];
extern u32 oracle_exi_getid_ok[3];
extern u32 oracle_exi_id[3];
extern u32 oracle_exi_card_status[3];
extern u32 oracle_exi_card_status_reads[3];
extern u32 oracle_exi_card_status_clears[3];

extern int oracle_card_unlock_ok[2];
extern u8 oracle_card_unlock_flash_id[2][12];
extern int oracle_card_unlock_calls[2];

extern void oracle_set_sram_flashid(int chan, const u8* id12);
extern void oracle_get_sram_flashid(int chan, u8* out12, u8* outchk);
extern void oracle_corrupt_sram_checksum(int chan, u8 xor_mask);

typedef struct {
  s32 result;
  s32 attached;
  s32 mount_step;
  s32 size_mb;
  s32 sector_size;
  u32 cid;
  u32 cblock;
  u32 latency;
  u8 id[12];
} GcCardControl;
extern GcCardControl gc_card_block[2];

static inline void wr32be(volatile u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline u32 h32(u32 h, u32 v) { return rotl1(h) ^ v; }

static u32 hash_card(u32 h, int chan) {
  h = h32(h, (u32)gc_card_block[chan].result);
  h = h32(h, (u32)gc_card_block[chan].attached);
  h = h32(h, (u32)gc_card_block[chan].mount_step);
  h = h32(h, (u32)gc_card_block[chan].size_mb);
  h = h32(h, (u32)gc_card_block[chan].sector_size);
  h = h32(h, (u32)gc_card_block[chan].cid);
  h = h32(h, (u32)gc_card_block[chan].cblock);
  h = h32(h, (u32)gc_card_block[chan].latency);
  for (int i = 0; i < 12; i++) h = h32(h, (u32)gc_card_block[chan].id[i]);
  return h;
}

static u32 hash_sram(u32 h, int chan) {
  u8 id12[12];
  u8 chk = 0;
  oracle_get_sram_flashid(chan, id12, &chk);
  for (int i = 0; i < 12; i++) h = h32(h, (u32)id12[i]);
  h = h32(h, (u32)chk);
  return h;
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (u8*)0x80000000u);
  volatile u8* out = (volatile u8*)0x80300000u;
  for (u32 i = 0; i < 0x40u; i++) out[i] = 0;

  // Prepare deterministic SRAM flashID for chan0.
  const u8 sram_id0[12] = { 1,2,3,4,5,6,7,8,9,10,11,12 };
  const u8 unlock_id0[12] = { 0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB };

  u32 l0 = 0, l1 = 0, l2 = 0;

  // Case 0: status has 0x40 set, SRAM checksum OK -> mount_step becomes 1.
  oracle_reset();
  *(volatile u8*)0x800030E3u = 0;
  oracle_exi_probeex_ret[0] = 1;
  oracle_exi_getid_ok[0] = 1;
  oracle_exi_id[0] = 0x00000004u;
  oracle_exi_card_status[0] = 0x40u;
  oracle_set_sram_flashid(0, sram_id0);
  s32 r0 = oracle_CARDMountAsync_step0(0);
  l0 = h32(l0, (u32)r0);
  l0 = hash_card(l0, 0);
  l0 = h32(l0, oracle_exi_card_status_reads[0]);
  l0 = h32(l0, oracle_exi_card_status_clears[0]);
  l0 = hash_sram(l0, 0);

  // Case 1: status lacks 0x40 -> unlock path writes SRAM flashID + checksum.
  oracle_reset();
  *(volatile u8*)0x800030E3u = 0;
  oracle_exi_probeex_ret[0] = 1;
  oracle_exi_getid_ok[0] = 1;
  oracle_exi_id[0] = 0x00000004u;
  oracle_exi_card_status[0] = 0x00u;
  oracle_card_unlock_ok[0] = 1;
  memcpy(oracle_card_unlock_flash_id[0], unlock_id0, 12);
  s32 r1 = oracle_CARDMountAsync_step0(0);
  l1 = h32(l1, (u32)r1);
  l1 = hash_card(l1, 0);
  l1 = h32(l1, (u32)oracle_card_unlock_calls[0]);
  l1 = hash_sram(l1, 0);

  // Case 2: status has 0x40 but SRAM checksum mismatch -> IOERROR.
  oracle_reset();
  *(volatile u8*)0x800030E3u = 0;
  oracle_exi_probeex_ret[0] = 1;
  oracle_exi_getid_ok[0] = 1;
  oracle_exi_id[0] = 0x00000004u;
  oracle_exi_card_status[0] = 0x40u;
  oracle_set_sram_flashid(0, sram_id0);
  oracle_corrupt_sram_checksum(0, 0x01u);
  s32 r2 = oracle_CARDMountAsync_step0(0);
  l2 = h32(l2, (u32)r2);
  l2 = hash_card(l2, 0);

  u32 master = 0;
  master = h32(master, l0);
  master = h32(master, l1);
  master = h32(master, l2);

  wr32be(out + 0x00, 0x4D533030u); // "MS00"
  wr32be(out + 0x04, master);
  wr32be(out + 0x08, l0);
  wr32be(out + 0x0C, l1);
  wr32be(out + 0x10, l2);

  while (1) {}
  return 0;
}
