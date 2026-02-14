#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_port/card/card_bios.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

enum {
  CARD_RESULT_BUSY = -1,
  PORT_CARD_MAX_FILE = 127,
  PORT_CARD_DIR_SIZE = 64,
  PORT_CARD_DIR_OFF_FILENAME = 8,
};

s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed);
s32 CARDGetSectorSize(s32 chan, u32* size);

static inline void wr16be(u8* p, u16 v) {
  p[0] = (u8)(v >> 8);
  p[1] = (u8)v;
}

static inline u32 mem_addr(u8* base, uintptr_t base_addr, uintptr_t ptr)
{
  return (u32)(ptr - (uintptr_t)base + base_addr);
}

static void reset_card_state(void)
{
  memset(gc_card_block, 0, sizeof(gc_card_block));
  gc_card_block[0].disk_id = 0x80000000u;
  gc_card_block[0].attached = 1;
  gc_card_block[0].result = 0;
  gc_card_block[0].sector_size = 8192;
}

static void write_files_freecount(u8* dir_base, u32 used_count)
{
  // Initialize all names as free.
  for (u32 i = 0; i < PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE; i++) {
    dir_base[i] = 0xFFu;
  }

  for (u32 i = 0; i < used_count; i++) {
    dir_base[i * PORT_CARD_DIR_SIZE + PORT_CARD_DIR_OFF_FILENAME] = 0x00u;
  }
}

static void reset_tables(u8* mem, u32 mem_size)
{
  (void)mem_size;
  memset(mem, 0, 0x5000u);
}

const char* gc_scenario_label(void) {
  return "CARDFreeBlocks/CARDGetSectorSize/pbt_001";
}

const char* gc_scenario_out_path(void) {
  return "../actual/card_free_blocks_pbt_001.bin";
}

void gc_scenario_run(GcRam* ram)
{
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x100u);
  if (!out) die("gc_ram_ptr(out) failed");
  memset(out, 0, 0x100u);

  u8* mem = gc_ram_ptr(ram, 0x80400000u, 0x5000u);
  if (!mem) die("gc_ram_ptr(mem) failed");
  reset_tables(mem, 0x5000u);

  u8* fat = mem + 0x0000u;
  u8* dir = mem + 0x2000u;
  gc_card_block[0].current_fat_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)fat);
  gc_card_block[0].current_dir_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)dir);

  u32 off = 0;
  s32 rc;
  s32 byteNotUsed = -1;
  s32 filesNotUsed = -1;
  u32 sectorSize = 0;
  const u16 freeBlocks = 7u;

  // L0: invalid channel.
  reset_card_state();
  gc_card_block[0].current_fat_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)fat);
  gc_card_block[0].current_dir_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)dir);
  reset_tables(mem, 0x5000u);
  wr16be(fat + 3u * sizeof(u16), freeBlocks);
  write_files_freecount(dir, 2u);
  byteNotUsed = -1;
  filesNotUsed = -1;
  rc = CARDFreeBlocks(-1, &byteNotUsed, &filesNotUsed);
  wr32be(out + off * 4u, 0x43465241u); // "CFBA"
  off++;
  wr32be(out + off * 4u, (u32)rc);
  off++;
  wr32be(out + off * 4u, (u32)byteNotUsed);
  off++;
  wr32be(out + off * 4u, (u32)filesNotUsed);
  off++;

  // L1: busy state.
  reset_card_state();
  gc_card_block[0].current_fat_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)fat);
  gc_card_block[0].current_dir_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)dir);
  gc_card_block[0].result = CARD_RESULT_BUSY;
  byteNotUsed = -1;
  filesNotUsed = -1;
  rc = CARDFreeBlocks(0, &byteNotUsed, &filesNotUsed);
  wr32be(out + off * 4u, 0x43465242u); // "CFBB"
  off++;
  wr32be(out + off * 4u, (u32)rc);
  off++;
  wr32be(out + off * 4u, (u32)byteNotUsed);
  off++;
  wr32be(out + off * 4u, (u32)filesNotUsed);
  off++;

  // L2: missing tables.
  reset_card_state();
  gc_card_block[0].current_fat_ptr = 0u;
  gc_card_block[0].current_dir_ptr = 0u;
  rc = CARDFreeBlocks(0, &byteNotUsed, &filesNotUsed);
  wr32be(out + off * 4u, 0x43465243u); // "CFBC"
  off++;
  wr32be(out + off * 4u, (u32)rc);
  off++;
  wr32be(out + off * 4u, (u32)byteNotUsed);
  off++;
  wr32be(out + off * 4u, (u32)filesNotUsed);
  off++;

  // L3: valid result.
  reset_card_state();
  gc_card_block[0].current_fat_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)fat);
  gc_card_block[0].current_dir_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)dir);
  gc_card_block[0].sector_size = 8192;
  wr16be(fat + 3u * sizeof(u16), freeBlocks);
  write_files_freecount(dir, 2u);
  rc = CARDFreeBlocks(0, &byteNotUsed, &filesNotUsed);
  wr32be(out + off * 4u, 0x43465244u); // "CFBD"
  off++;
  wr32be(out + off * 4u, (u32)rc);
  off++;
  wr32be(out + off * 4u, (u32)byteNotUsed);
  off++;
  wr32be(out + off * 4u, (u32)filesNotUsed);
  off++;

  // L4: sector size invalid chan.
  reset_card_state();
  sectorSize = 0xDEADBEEFu;
  rc = CARDGetSectorSize(-1, &sectorSize);
  wr32be(out + off * 4u, 0x43465331u); // "CFS1"
  off++;
  wr32be(out + off * 4u, (u32)rc);
  off++;
  wr32be(out + off * 4u, sectorSize);
  off++;

  // L5: sector size valid.
  reset_card_state();
  gc_card_block[0].current_fat_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)fat);
  gc_card_block[0].current_dir_ptr = mem_addr(mem, 0x80400000u, (uintptr_t)dir);
  gc_card_block[0].sector_size = 16384;
  rc = CARDGetSectorSize(0, &sectorSize);
  wr32be(out + off * 4u, 0x43465332u); // "CFS2"
  off++;
  wr32be(out + off * 4u, (u32)rc);
  off++;
  wr32be(out + off * 4u, sectorSize);
  off++;

  // L6: sector size busy.
  reset_card_state();
  gc_card_block[0].result = CARD_RESULT_BUSY;
  sectorSize = 0x12345678u;
  rc = CARDGetSectorSize(0, &sectorSize);
  wr32be(out + off * 4u, 0x43465333u); // "CFS3"
  off++;
  wr32be(out + off * 4u, (u32)rc);
  off++;
  wr32be(out + off * 4u, sectorSize);
  off++;

  wr32be(out + 0x00u, 0x43464253u); // "CBFS"
  wr32be(out + 0x04u, 0x00000000u + off);
}
