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

// Under test.
s32 CARDOpen(int32_t chan, const char* fileName, CARDFileInfo* fileInfo);
s32 CARDClose(CARDFileInfo* fileInfo);

static inline void wr16be(u8* p, u16 v) {
  p[0] = (u8)(v >> 8);
  p[1] = (u8)v;
}

static inline void init_empty_dir_entry(u8* ent) {
  for (u32 i = 0; i < 64u; i++) {
    ent[i] = 0xFFu;
  }
}

static void init_named_file(u8* ent, const char* name, u16 startBlock) {
  init_empty_dir_entry(ent);
  ent[0] = 0x00u;
  ent[4] = 0x00u;
  for (u32 i = 0; i < 32u; i++) {
    ent[8u + i] = (u8)name[i];
    if (name[i] == '\0') {
      break;
    }
  }
  wr16be(ent + 54u, startBlock);
}

static void dump_case(u8* out, u32* w, u32 case_id, s32 ret, const CARDFileInfo* fi) {
  wr32be(out + ((*w)++ * 4u), case_id);
  wr32be(out + ((*w)++ * 4u), (u32)ret);
  wr32be(out + ((*w)++ * 4u), (u32)fi->chan);
  wr32be(out + ((*w)++ * 4u), (u32)fi->fileNo);
  wr32be(out + ((*w)++ * 4u), (u32)fi->offset);
  wr32be(out + ((*w)++ * 4u), (u32)fi->iBlock);
  wr32be(out + ((*w)++ * 4u), (u32)gc_card_block[0].result);
}

const char* gc_scenario_label(void) {
  return "CARDOpen/CARDClose/pbt_001";
}

const char* gc_scenario_out_path(void) {
  return "../actual/card_open_pbt_001.bin";
}

void gc_scenario_run(GcRam* ram) {
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x100u);
  if (!out) {
    die("gc_ram_ptr(out) failed");
  }

  u8* work = gc_ram_ptr(ram, 0x80400000u, 0x2000u);
  if (!work) {
    die("gc_ram_ptr(work) failed");
  }

  memset(out, 0, 0x100u);
  memset(work, 0, 0x2000u);
  memset(gc_card_block, 0, sizeof(gc_card_block));

  gc_card_block[0].disk_id = 0x80000000u;
  gc_card_block[0].attached = 1;
  gc_card_block[0].cblock = 64;
  gc_card_block[0].sector_size = 8192;
  gc_card_block[0].work_area = (uintptr_t)work;
  gc_card_block[0].current_dir_ptr = 0x80400000u;
  gc_card_block[0].current_fat_ptr = 0;

  init_named_file(work + 0u, "savefile.bin", 5u);
  init_named_file(work + 64u, "badstart.bin", 3u);

  CARDFileInfo fi;
  u32 w = 0u;

  wr32be(out + w * 4u, 0x434f5030u);
  w++;
  wr32be(out + w * 4u, 7u);
  w++;

  memset(&fi, 0x55u, sizeof(fi));
  s32 rc0 = CARDOpen(0, "savefile.bin", &fi);
  dump_case(out, &w, 0u, rc0, &fi);

  memset(&fi, 0x55u, sizeof(fi));
  s32 rc1 = CARDOpen(0, "missing.bin", &fi);
  dump_case(out, &w, 1u, rc1, &fi);

  memset(&fi, 0x55u, sizeof(fi));
  s32 rc2 = CARDOpen(0, "badstart.bin", &fi);
  dump_case(out, &w, 2u, rc2, &fi);

  memset(&fi, 0x55u, sizeof(fi));
  s32 rc3 = CARDOpen(-1, "savefile.bin", &fi);
  dump_case(out, &w, 3u, rc3, &fi);

  gc_card_block[0].attached = 0;
  memset(&fi, 0x55u, sizeof(fi));
  s32 rc4 = CARDOpen(0, "savefile.bin", &fi);
  dump_case(out, &w, 4u, rc4, &fi);

  memset(&fi, 0x55u, sizeof(fi));
  gc_card_block[0].attached = 1;
  gc_card_block[0].result = 0;
  s32 rc5_open = CARDOpen(0, "savefile.bin", &fi);
  (void)rc5_open;
  s32 rc5 = CARDClose(&fi);
  dump_case(out, &w, 5u, rc5, &fi);

  fi.chan = -1;
  s32 rc6 = CARDClose((CARDFileInfo*)&fi);
  dump_case(out, &w, 6u, rc6, &fi);
}
