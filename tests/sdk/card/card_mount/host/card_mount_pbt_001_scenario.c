#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/exi.h"
#include "dolphin/OSRtcPriv.h"

#include "sdk_port/card/card_bios.h"
#include "sdk_port/card/memcard_backend.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

s32 CARDMountAsync(s32 chan, void* workArea, void (*detachCallback)(s32, s32), void (*attachCallback)(s32, s32));

static u32 fnv1a32(const u8* p, u32 n) {
  u32 h = 2166136261u;
  for (u32 i = 0; i < n; i++) {
    h ^= (u32)p[i];
    h *= 16777619u;
  }
  return h;
}

static inline void wr16be(u8* p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static inline u16 rd16be(const u8* p) { return (u16)(((u16)p[0] << 8) | (u16)p[1]); }

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

enum { CARD_SYSTEM_BLOCK_SIZE = 8 * 1024 };
enum { CARD_MAX_FILE = 127 };
enum { CARD_DIR_SIZE = 64 };
enum { CARD_NUM_SYSTEM_BLOCK = 5 };
enum { CARD_FAT_AVAIL = 0x0000u, CARD_FAT_CHECKSUM = 0, CARD_FAT_CHECKSUMINV = 1, CARD_FAT_CHECKCODE = 2, CARD_FAT_FREEBLOCKS = 3 };

static void make_dir_block(u8* base, int16_t checkCode, u8 seed) {
  for (u32 i = 0; i < (u32)CARD_SYSTEM_BLOCK_SIZE; i++) base[i] = (u8)(seed ^ (u8)i);
  u8* chk = base + (u32)CARD_MAX_FILE * CARD_DIR_SIZE;
  wr16be(chk + (CARD_DIR_SIZE - 6), (u16)checkCode);
  u16 cs = 0, csi = 0;
  card_checksum_u16be(base, (u32)CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
  wr16be(chk + (CARD_DIR_SIZE - 4), cs);
  wr16be(chk + (CARD_DIR_SIZE - 2), csi);
}

static void make_fat_block(u8* base, u16 checkCode, u16 cBlock) {
  memset(base, 0, CARD_SYSTEM_BLOCK_SIZE);
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

static void make_id_block(u8* base, u16 size_u16, u16 encode, const u8* flash_id12) {
  memset(base, 0, 512u);
  for (u32 i = 0; i < 12u; i++) base[i] = flash_id12[i];
  wr16be(base + 32u, 0);
  wr16be(base + 34u, size_u16);
  wr16be(base + 36u, encode);
  u16 cs = 0, csi = 0;
  card_checksum_u16be(base, 512u - 4u, &cs, &csi);
  wr16be(base + 508u, cs);
  wr16be(base + 510u, csi);
}

static s32 g_attach_calls;
static s32 g_attach_last;
static void attach_cb(s32 chan, s32 result) { (void)chan; g_attach_calls++; g_attach_last = result; }

const char* gc_scenario_label(void) { return "CARDMountAsync/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_mount_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x100u);
  if (!out) die("gc_ram_ptr(out) failed");
  memset(out, 0, 0x100u);
  u32 off = 0;

  // Workarea in GC RAM.
  u8* work = gc_ram_ptr(ram, 0x80400000u, 5u * 8u * 1024u);
  if (!work) die("gc_ram_ptr(work) failed");
  memset(work, 0, 5u * 8u * 1024u);

  // Seed SRAMEx flashID (so __CARDVerify passes with seed=0).
  __OSInitSram();
  OSSramEx* sramEx = __OSLockSramEx();
  if (!sramEx) die("__OSLockSramEx failed");
  u8 flash0[12];
  for (u32 i = 0; i < 12u; i++) {
    flash0[i] = (u8)(0x10u + (u8)i);
    sramEx->flashID[0][i] = flash0[i];
  }
  u8 sum = 0;
  for (u32 i = 0; i < 12u; i++) sum = (u8)(sum + sramEx->flashID[0][i]);
  sramEx->flashIDCheckSum[0] = (u8)~sum;
  (void)__OSUnlockSramEx(0);

  // Create deterministic memcard image containing the 5 system blocks at offsets 0,0x2000,... (sectorSize=8KiB).
  const char* mc_path = "../actual/card_mount_pbt_001_memcard.raw";
  const u32 mc_size = 0x40000u;
  if (gc_memcard_insert(0, mc_path, mc_size) != 0) die("gc_memcard_insert failed");

  u8 blk[0x2000];
  // block0: CARDID
  memset(blk, 0, sizeof(blk));
  make_id_block(blk, 0x0004u, 0u, flash0);
  if (gc_memcard_write(0, 0x0000u, blk, sizeof(blk)) != 0) die("write id failed");
  // block1/2: dirs
  make_dir_block(blk, 1, 0xA0);
  if (gc_memcard_write(0, 0x2000u, blk, sizeof(blk)) != 0) die("write dir0 failed");
  make_dir_block(blk, 2, 0xB0);
  if (gc_memcard_write(0, 0x4000u, blk, sizeof(blk)) != 0) die("write dir1 failed");
  // block3/4: fats
  make_fat_block(blk, 1, 64u);
  if (gc_memcard_write(0, 0x6000u, blk, sizeof(blk)) != 0) die("write fat0 failed");
  make_fat_block(blk, 2, 64u);
  if (gc_memcard_write(0, 0x8000u, blk, sizeof(blk)) != 0) die("write fat1 failed");

  // Configure EXI model.
  EXIInit();
  gc_exi_dma_hook = gc_memcard_exi_dma;
  gc_exi_probeex_ret[0] = 1;
  gc_exi_getid_ok[0] = 1;
  gc_exi_id[0] = 0x00000004u;
  gc_exi_card_status[0] = 0x40u;

  // Clear modeled CARD state.
  memset(gc_card_block, 0, sizeof(gc_card_block));
  gc_card_block[0].attached = 1;
  gc_card_block[0].work_area = (uintptr_t)work;
  gc_card_block[0].current_dir_ptr = 0;
  gc_card_block[0].current_fat_ptr = 0;

  g_attach_calls = 0;
  g_attach_last = 0;
  s32 rc = CARDMountAsync(0, work, attach_cb, attach_cb);

  u32 h0 = fnv1a32(work + 0x0000u, 0x2000u);
  u32 h1 = fnv1a32(work + 0x2000u, 0x2000u);
  u32 h2 = fnv1a32(work + 0x4000u, 0x2000u);
  u32 h3 = fnv1a32(work + 0x6000u, 0x2000u);
  u32 h4 = fnv1a32(work + 0x8000u, 0x2000u);

  wr32be(out + off, 0x434D5430u); off += 4;
  wr32be(out + off, (u32)rc); off += 4;
  wr32be(out + off, (u32)gc_card_block[0].mount_step); off += 4;
  wr32be(out + off, (u32)gc_card_block[0].result); off += 4;
  wr32be(out + off, (u32)g_attach_calls); off += 4;
  wr32be(out + off, (u32)g_attach_last); off += 4;
  wr32be(out + off, h0); off += 4;
  wr32be(out + off, h1); off += 4;
  wr32be(out + off, h2); off += 4;
  wr32be(out + off, h3); off += 4;
  wr32be(out + off, h4); off += 4;
}

