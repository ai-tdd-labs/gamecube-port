#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_port/card/card_bios.h"
#include "sdk_port/card/card_dir.h"
#include "sdk_port/card/card_fat.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };
enum { CARD_NUM_SYSTEM_BLOCK = 5u };

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_NAMETOOLONG = -12,
};

s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, void (*callback)(s32, s32));

static inline void wr16be(u8* p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static inline u16 rd16be(const u8* p) { return (u16)(((u16)p[0] << 8u) | (u16)p[1]); }

static u32 fnv1a32(const u8* p, u32 n) {
  u32 h = 2166136261u;
  for (u32 i = 0; i < n; i++) { h ^= (u32)p[i]; h *= 16777619u; }
  return h;
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

static inline u16 fat_get(const u8* fat, u16 idx) { return rd16be(fat + (u32)idx * 2u); }
static inline void fat_set(u8* fat, u16 idx, u16 v) { wr16be(fat + (u32)idx * 2u, v); }

static void init_dir(u8* dir, u16 checkCode)
{
  memset(dir, 0xFFu, CARD_SYSTEM_BLOCK_SIZE);
  u8* chk = dir + (u32)PORT_CARD_MAX_FILE * (u32)PORT_CARD_DIR_SIZE;
  wr16be(chk + (PORT_CARD_DIR_SIZE - 6u), checkCode);
  u16 cs = 0, csi = 0;
  card_checksum_u16be(dir, CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
  wr16be(chk + (PORT_CARD_DIR_SIZE - 4u), cs);
  wr16be(chk + (PORT_CARD_DIR_SIZE - 2u), csi);
}

static void init_fat(u8* fat, u16 checkCode, u16 cBlock)
{
  memset(fat, 0x00u, CARD_SYSTEM_BLOCK_SIZE);
  for (u16 b = (u16)CARD_NUM_SYSTEM_BLOCK; b < cBlock; b++) {
    fat_set(fat, b, PORT_CARD_FAT_AVAIL);
  }
  fat_set(fat, (u16)PORT_CARD_FAT_CHECKCODE, checkCode);
  fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, (u16)(cBlock - (u16)CARD_NUM_SYSTEM_BLOCK));
  fat_set(fat, (u16)PORT_CARD_FAT_LASTSLOT, (u16)((u16)CARD_NUM_SYSTEM_BLOCK - 1u));

  u16 cs = 0, csi = 0;
  card_checksum_u16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
  fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUM, cs);
  fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUMINV, csi);
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result) { (void)chan; g_cb_calls++; g_cb_last = result; }

const char* gc_scenario_label(void) { return "CARDCreate/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_create_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram)
{
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x200u);
  if (!out) die("gc_ram_ptr(out) failed");
  memset(out, 0, 0x200u);

  u8* dir = gc_ram_ptr(ram, 0x80400000u, CARD_SYSTEM_BLOCK_SIZE);
  u8* fat = gc_ram_ptr(ram, 0x80402000u, CARD_SYSTEM_BLOCK_SIZE);
  if (!dir || !fat) die("gc_ram_ptr(dir/fat) failed");

  memset(gc_card_block, 0, sizeof(gc_card_block));
  gc_card_block[0].disk_id = 0x80000000u;
  gc_card_block[0].attached = 1;
  gc_card_block[0].result = CARD_RESULT_READY;
  gc_card_block[0].sector_size = 8192;
  gc_card_block[0].cblock = 64;
  memcpy(gc_card_block[0].id, "GAMEC0", 6);

  init_dir(dir, 0);
  init_fat(fat, 0, 64);

  gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
  gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

  // Seed one existing entry that should collide on name.
  u8* ent0 = dir + 0u * (u32)PORT_CARD_DIR_SIZE;
  memcpy(ent0 + PORT_CARD_DIR_OFF_GAMENAME, gc_card_block[0].id, 4);
  memcpy(ent0 + PORT_CARD_DIR_OFF_COMPANY, gc_card_block[0].id + 4, 2);
  memset(ent0 + PORT_CARD_DIR_OFF_FILENAME, 0, PORT_CARD_FILENAME_MAX);
  memcpy(ent0 + PORT_CARD_DIR_OFF_FILENAME, "dup", 4);

  u32 w = 0;
  wr32be(out + w, 0x43435230u); w += 4; // CCR0

  // Case A: success.
  CARDFileInfo fi;
  memset(&fi, 0xE5u, sizeof(fi));
  g_cb_calls = 0;
  g_cb_last = 0;
  s32 rc = CARDCreateAsync(0, "ok", 2u * 8192u, &fi, cb_record);

  wr32be(out + w, 0x43435231u); w += 4;
  wr32be(out + w, (u32)rc); w += 4;
  wr32be(out + w, (u32)g_cb_calls); w += 4;
  wr32be(out + w, (u32)g_cb_last); w += 4;
  wr32be(out + w, (u32)gc_card_block[0].result); w += 4;
  wr32be(out + w, (u32)gc_card_block[0].freeNo); w += 4;
  wr32be(out + w, (u32)gc_card_block[0].startBlock); w += 4;
  wr32be(out + w, (u32)fi.fileNo); w += 4;
  wr32be(out + w, (u32)fi.offset); w += 4;
  wr32be(out + w, (u32)fi.iBlock); w += 4;
  wr32be(out + w, fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
  wr32be(out + w, fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE)); w += 4;

  // Case B: duplicate.
  memset(&fi, 0xE5u, sizeof(fi));
  g_cb_calls = 0;
  g_cb_last = 0;
  u32 h_dir_before = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);
  u32 h_fat_before = fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE);
  rc = CARDCreateAsync(0, "dup", 1u * 8192u, &fi, cb_record);

  wr32be(out + w, 0x43435232u); w += 4;
  wr32be(out + w, (u32)rc); w += 4;
  wr32be(out + w, (u32)g_cb_calls); w += 4;
  wr32be(out + w, (u32)g_cb_last); w += 4;
  wr32be(out + w, fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
  wr32be(out + w, fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
  wr32be(out + w, h_dir_before); w += 4;
  wr32be(out + w, h_fat_before); w += 4;

  // Case C: insufficient space.
  fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, 0);
  g_cb_calls = 0;
  rc = CARDCreateAsync(0, "ns", 1u * 8192u, &fi, cb_record);
  wr32be(out + w, 0x43435233u); w += 4;
  wr32be(out + w, (u32)rc); w += 4;
  wr32be(out + w, (u32)g_cb_calls); w += 4;

  // Case D: name too long.
  rc = CARDCreateAsync(0, "0123456789012345678901234567890123", 1u * 8192u, &fi, cb_record);
  wr32be(out + w, 0x43435234u); w += 4;
  wr32be(out + w, (u32)rc); w += 4;
  (void)CARD_RESULT_NAMETOOLONG;
}
