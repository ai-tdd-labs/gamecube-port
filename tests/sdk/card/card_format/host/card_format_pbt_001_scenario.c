#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_port/card/card_bios.h"
#include "sdk_port/card/card_dir.h"
#include "sdk_port/card/card_fat.h"
#include "sdk_port/card/card_mem.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

typedef void (*CARDCallback)(s32 chan, s32 result);

enum { CARD_NUM_SYSTEM_BLOCK = 5u };
enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };

enum {
  CARD_RESULT_READY = 0,
};

// Not exported in card_bios.h; exercised directly for fixed-encode determinism.
s32 __CARDFormatRegionAsync(s32 chan, u16 encode, CARDCallback callback);

static inline u16 rd16be(const u8* p) { return (u16)(((u16)p[0] << 8u) | (u16)p[1]); }

static u32 fnv1a32(const u8* p, u32 n) {
  u32 h = 2166136261u;
  for (u32 i = 0; i < n; i++) { h ^= (u32)p[i]; h *= 16777619u; }
  return h;
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result) { (void)chan; g_cb_calls++; g_cb_last = result; }

const char* gc_scenario_label(void) { return "CARDFormat/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_format_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram)
{
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x200u);
  if (!out) die("gc_ram_ptr(out) failed");
  memset(out, 0, 0x200u);

  const u32 work_size = (u32)CARD_NUM_SYSTEM_BLOCK * (u32)CARD_SYSTEM_BLOCK_SIZE;
  u8* work = gc_ram_ptr(ram, 0x80400000u, work_size);
  if (!work) die("gc_ram_ptr(work) failed");

  memset(gc_card_block, 0, sizeof(gc_card_block));
  gc_card_block[0].disk_id = 0x80000000u;
  gc_card_block[0].attached = 1;
  gc_card_block[0].result = CARD_RESULT_READY;
  gc_card_block[0].sector_size = 8192;
  gc_card_block[0].cblock = 64;
  gc_card_block[0].size_u16 = 0x0020u;
  gc_card_block[0].work_area = 0x80400000u;

  u32 w = 0;
  wr32be(out + w, 0x43465230u); w += 4; // CFR0

  // Case A: success.
  g_cb_calls = 0;
  g_cb_last = 0;
  s32 rc = __CARDFormatRegionAsync(0, 0x1234u, cb_record);

  u8* cur_dir = (u8*)card_ptr_ro(gc_card_block[0].current_dir_ptr, CARD_SYSTEM_BLOCK_SIZE);
  u8* cur_fat = (u8*)card_ptr_ro(gc_card_block[0].current_fat_ptr, CARD_SYSTEM_BLOCK_SIZE);
  u16 dir_cc = cur_dir ? rd16be(cur_dir + (u32)PORT_CARD_MAX_FILE * (u32)PORT_CARD_DIR_SIZE + (PORT_CARD_DIR_SIZE - 6u)) : 0;
  u16 fat_cc = cur_fat ? rd16be(cur_fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u) : 0;
  u16 id_encode = rd16be(work + 36u);

  wr32be(out + w, 0x43465231u); w += 4;
  wr32be(out + w, (u32)rc); w += 4;
  wr32be(out + w, (u32)g_cb_calls); w += 4;
  wr32be(out + w, (u32)g_cb_last); w += 4;
  wr32be(out + w, (u32)gc_card_block[0].result); w += 4;
  wr32be(out + w, (u32)dir_cc); w += 4;
  wr32be(out + w, (u32)fat_cc); w += 4;
  wr32be(out + w, (u32)id_encode); w += 4;
  wr32be(out + w, fnv1a32(work, work_size)); w += 4;

  // Case B: broken (null work_area).
  gc_card_block[0].result = CARD_RESULT_READY;
  gc_card_block[0].work_area = 0u;
  g_cb_calls = 0;
  rc = __CARDFormatRegionAsync(0, 0x1234u, cb_record);
  wr32be(out + w, 0x43465232u); w += 4;
  wr32be(out + w, (u32)rc); w += 4;
  wr32be(out + w, (u32)g_cb_calls); w += 4;
}
