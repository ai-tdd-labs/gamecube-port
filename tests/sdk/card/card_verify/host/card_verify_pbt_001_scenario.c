#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/OSRtcPriv.h"

#include "sdk_port/card/card_bios.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

// Under test (to be implemented in sdk_port).
s32 __CARDVerify(GcCardControl* card);

enum { CARD_SYSTEM_BLOCK_SIZE = 8 * 1024 };
enum { CARD_NUM_SYSTEM_BLOCK = 5 };
enum { CARD_MAX_FILE = 127 };
enum { CARD_FAT_AVAIL = 0x0000u, CARD_FAT_CHECKSUM = 0, CARD_FAT_CHECKSUMINV = 1, CARD_FAT_CHECKCODE = 2, CARD_FAT_FREEBLOCKS = 3 };

static inline void wr16be(u8* p, u16 v) {
  p[0] = (u8)(v >> 8);
  p[1] = (u8)(v);
}
static inline u16 rd16be(const u8* p) {
  return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static void card_checksum_u16be(const u8* ptr, u32 length, u16* checksum, u16* checksumInv) {
  u32 n = length / 2u;
  u16 cs = 0, csi = 0;
  for (u32 i = 0; i < n; i++) {
    u16 v = rd16be(ptr + i * 2u);
    cs = (u16)(cs + v);
    csi = (u16)(csi + (u16)~v);
  }
  if (cs == 0xFFFF) cs = 0;
  if (csi == 0xFFFF) csi = 0;
  *checksum = cs;
  *checksumInv = csi;
}

static u32 fnv1a32(const u8* p, u32 n) {
  u32 h = 2166136261u;
  for (u32 i = 0; i < n; i++) {
    h ^= (u32)p[i];
    h *= 16777619u;
  }
  return h;
}

static void make_dir_block(u8* base, int16_t checkCode, u8 seed) {
  for (u32 i = 0; i < (u32)CARD_SYSTEM_BLOCK_SIZE; i++) base[i] = (u8)(seed ^ (u8)i);
  u8* chk = base + (u32)CARD_MAX_FILE * 64u; // sizeof(CARDDir)=64
  // checkCode at offset (64-6) within CARDDirCheck, checksum at (64-4), inv at (64-2)
  wr16be(chk + (64u - 6u), (u16)checkCode);
  u16 cs = 0, csi = 0;
  card_checksum_u16be(base, (u32)CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
  wr16be(chk + (64u - 4u), cs);
  wr16be(chk + (64u - 2u), csi);
}

static void make_fat_block(u8* base, u16 checkCode, u16 cBlock) {
  memset(base, 0, CARD_SYSTEM_BLOCK_SIZE);
  // Mark all user blocks as available.
  for (u16 b = CARD_NUM_SYSTEM_BLOCK; b < cBlock; b++) {
    wr16be(base + (u32)b * 2u, CARD_FAT_AVAIL);
  }
  wr16be(base + (u32)CARD_FAT_CHECKCODE * 2u, checkCode);
  wr16be(base + (u32)CARD_FAT_FREEBLOCKS * 2u, (u16)(cBlock - CARD_NUM_SYSTEM_BLOCK));

  u16 cs = 0, csi = 0;
  card_checksum_u16be(base + (u32)CARD_FAT_CHECKCODE * 2u, (u32)CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
  wr16be(base + (u32)CARD_FAT_CHECKSUM * 2u, cs);
  wr16be(base + (u32)CARD_FAT_CHECKSUMINV * 2u, csi);
}

static void make_id(u8* base, u16 size, u16 encode, const u8* flash_id12) {
  memset(base, 0, 512u);
  // serial[0..11]
  for (u32 i = 0; i < 12u; i++) base[i] = flash_id12[i];
  // deviceID @ serial[32..33]
  wr16be(base + 32u, 0);
  // size @ serial[34..35]
  wr16be(base + 34u, size);
  // encode @ serial[36..37]
  wr16be(base + 36u, encode);
  // checksums at end of struct (offset 508/510)
  u16 cs = 0, csi = 0;
  card_checksum_u16be(base, 512u - 4u, &cs, &csi);
  wr16be(base + 508u, cs);
  wr16be(base + 510u, csi);
}

const char* gc_scenario_label(void) { return "__CARDVerify/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_verify_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x80u);
  if (!out) die("gc_ram_ptr(out) failed");
  memset(out, 0, 0x80u);
  u32 off = 0;

  // Work area.
  u8* work = gc_ram_ptr(ram, 0x80400000u, 5u * 8u * 1024u);
  if (!work) die("gc_ram_ptr(work) failed");
  memset(work, 0, 5u * 8u * 1024u);

  // Fill SRAMEx flashID via existing host model knobs (OS SRAM suite).
  // Mirror the DOL oracle values (seed=0 => LCG yields 0 so serial[i]==flashID[i]).
  u8 flash0[12];
  for (u32 i = 0; i < 12u; i++) flash0[i] = (u8)(0x10u + (u8)i);
  __OSInitSram();
  OSSramEx* sramEx = __OSLockSramEx();
  if (!sramEx) die("__OSLockSramEx failed");
  for (u32 i = 0; i < 12u; i++) sramEx->flashID[0][i] = flash0[i];
  (void)__OSUnlockSramEx(0);

  // Setup card.
  memset(gc_card_block, 0, sizeof(gc_card_block));
  GcCardControl* card = &gc_card_block[0];
  card->attached = 1;
  card->cblock = 0x20;
  card->work_area = (uintptr_t)work;
  card->size_mb = 0; // unused by verify

  // Case A: pass + select/copy.
  make_id(work + 0x0000u, 0x1234u, 0u, flash0);
  make_dir_block(work + 0x2000u, 1, 0xA0);
  make_dir_block(work + 0x4000u, 2, 0xB0);
  make_fat_block(work + 0x6000u, 1, (u16)card->cblock);
  make_fat_block(work + 0x8000u, 2, (u16)card->cblock);
  card->current_dir_ptr = 0;
  card->current_fat_ptr = 0;
  card->size_u16 = 0x1234u;
  s32 rc_a = __CARDVerify(card);
  u32 dir0_h = fnv1a32(work + 0x2000u, CARD_SYSTEM_BLOCK_SIZE);
  u32 fat0_h = fnv1a32(work + 0x6000u, CARD_SYSTEM_BLOCK_SIZE);
  wr32be(out + off, 0x43564641u); off += 4;
  wr32be(out + off, (u32)rc_a); off += 4;
  wr32be(out + off, (u32)(card->current_dir_ptr == (uintptr_t)(work + 0x2000u))); off += 4;
  wr32be(out + off, (u32)(card->current_fat_ptr == (uintptr_t)(work + 0x6000u))); off += 4;
  wr32be(out + off, dir0_h); off += 4;
  wr32be(out + off, fat0_h); off += 4;

  // Case B: ID checksum fail -> BROKEN.
  make_id(work + 0x0000u, 0x1234u, 0u, flash0);
  work[508] ^= 0x12;
  s32 rc_b = __CARDVerify(card);
  wr32be(out + off, 0x43564642u); off += 4;
  wr32be(out + off, (u32)rc_b); off += 4;

  // Case C: encoding fail.
  make_id(work + 0x0000u, 0x1234u, 1u, flash0);
  s32 rc_c = __CARDVerify(card);
  wr32be(out + off, 0x43564643u); off += 4;
  wr32be(out + off, (u32)rc_c); off += 4;

  wr32be(out + 0x00, 0x43564630u);
}
