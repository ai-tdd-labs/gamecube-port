#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/types.h"
#include "dolphin/exi.h"

#include "sdk_port/card/card_bios.h"
#include "sdk_port/card/card_dir.h"
#include "sdk_port/card/card_fat.h"
#include "sdk_port/card/memcard_backend.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };
enum { CARD_NUM_SYSTEM_BLOCK = 5u };

enum { CARD_RESULT_READY = 0 };

static inline void wr16be(u8* p, u16 v) { p[0] = (u8)(v >> 8u); p[1] = (u8)v; }
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

static void init_dir(u8* dir)
{
  memset(dir, 0xFFu, CARD_SYSTEM_BLOCK_SIZE);
  u8* chk = dir + (u32)PORT_CARD_MAX_FILE * (u32)PORT_CARD_DIR_SIZE;
  wr16be(chk + (PORT_CARD_DIR_SIZE - 6u), 0);
  u16 cs = 0, csi = 0;
  card_checksum_u16be(dir, CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
  wr16be(chk + (PORT_CARD_DIR_SIZE - 4u), cs);
  wr16be(chk + (PORT_CARD_DIR_SIZE - 2u), csi);
}

static void init_fat(u8* fat)
{
  memset(fat, 0x00u, CARD_SYSTEM_BLOCK_SIZE);
  for (u16 b = (u16)CARD_NUM_SYSTEM_BLOCK; b < 64u; b++) fat_set(fat, b, PORT_CARD_FAT_AVAIL);
}

static void seed_entry(u8* dir, const u8* id6, u8 perm, u16 startBlock, u16 lenBlocks)
{
  u8* ent = dir + 0u * (u32)PORT_CARD_DIR_SIZE;
  memcpy(ent + PORT_CARD_DIR_OFF_GAMENAME, id6, 4);
  memcpy(ent + PORT_CARD_DIR_OFF_COMPANY, id6 + 4, 2);
  ent[PORT_CARD_DIR_OFF_PERMISSION] = perm;
  wr16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK, startBlock);
  wr16be(ent + PORT_CARD_DIR_OFF_LENGTH, lenBlocks);
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result) { (void)chan; g_cb_calls++; g_cb_last = result; }

static void fill_memcard(int chan, u32 size)
{
  u8 tmp[512];
  for (u32 i = 0; i < size; i += (u32)sizeof(tmp)) {
    for (u32 j = 0; j < (u32)sizeof(tmp); j++) {
      u32 x = i + j;
      tmp[j] = (u8)((x ^ (x >> 8) ^ 0xA5u) & 0xFFu);
    }
    if (gc_memcard_write(chan, i, tmp, (u32)sizeof(tmp)) != 0) die("gc_memcard_write fill failed");
  }
}

const char* gc_scenario_label(void) { return "CARDRead/api_pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_read_api_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram)
{
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x200u);
  if (!out) die("gc_ram_ptr(out) failed");
  memset(out, 0, 0x200u);
  u32 w = 0;
  wr32be(out + w, 0x43525230u); w += 4; // CRR0

  // Configure memcard backing store.
  const char* mc_path = "../actual/card_read_api_pbt_001_memcard.raw";
  const u32 mc_size = 0x40000u;
  if (gc_memcard_insert(0, mc_path, mc_size) != 0) die("gc_memcard_insert failed");
  fill_memcard(0, mc_size);

  // Setup EXI model.
  EXIInit();
  gc_exi_probeex_ret[0] = 1;
  gc_exi_dma_hook = gc_memcard_exi_dma;

  // Provide a work area for __CARDReadSegment (work_area + 512 pointer math).
  u8* work = gc_ram_ptr(ram, 0x80400000u, 0x800u);
  if (!work) die("gc_ram_ptr(work) failed");
  memset(work, 0, 0x800u);

  // Common state.
  memset(gc_card_block, 0, sizeof(gc_card_block));
  gc_card_block[0].disk_id = 0x80000000u;
  gc_card_block[0].attached = 1;
  gc_card_block[0].result = CARD_RESULT_READY;
  gc_card_block[0].sector_size = 8192;
  gc_card_block[0].cblock = 64;
  gc_card_block[0].work_area = (uintptr_t)work;
  gc_card_block[0].latency = 0;
  memcpy(gc_card_block[0].id, "GAMEC0", 6);

  // Case A: read 1024 @ offset 0.
  {
    u8* dir = gc_ram_ptr(ram, 0x80401000u, CARD_SYSTEM_BLOCK_SIZE);
    u8* fat = gc_ram_ptr(ram, 0x80403000u, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir || !fat) die("gc_ram_ptr(dir/fat) failed");
    init_dir(dir);
    init_fat(fat);
    seed_entry(dir, (const u8*)"GAMEC0", (u8)PORT_CARD_ATTR_PUBLIC, 5, 2);
    fat_set(fat, 5, 6);
    fat_set(fat, 6, 0xFFFFu);
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    u8* buf = gc_ram_ptr(ram, 0x80408000u, 1024u);
    if (!buf) die("gc_ram_ptr(bufA) failed");
    memset(buf, 0xEEu, 1024u);

    CARDFileInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.chan = 0;
    fi.fileNo = 0;
    fi.offset = 0;
    fi.iBlock = 5;

    g_cb_calls = 0;
    g_cb_last = 0;
    s32 rc = CARDReadAsync(&fi, buf, 1024, 0, cb_record);
    u32 h = fnv1a32(buf, 1024u);

    wr32be(out + w, 0x43525231u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, h); w += 4;
  }

  // Case B: read 1024 @ offset 7680.
  {
    u8* dir = gc_ram_ptr(ram, 0x8040A000u, CARD_SYSTEM_BLOCK_SIZE);
    u8* fat = gc_ram_ptr(ram, 0x8040C000u, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir || !fat) die("gc_ram_ptr(dir/fat) failed");
    init_dir(dir);
    init_fat(fat);
    seed_entry(dir, (const u8*)"GAMEC0", (u8)PORT_CARD_ATTR_PUBLIC, 5, 2);
    fat_set(fat, 5, 6);
    fat_set(fat, 6, 0xFFFFu);
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    u8* buf = gc_ram_ptr(ram, 0x80412000u, 1024u);
    if (!buf) die("gc_ram_ptr(bufB) failed");
    memset(buf, 0xDDu, 1024u);

    CARDFileInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.chan = 0;
    fi.fileNo = 0;
    fi.offset = 0;
    fi.iBlock = 5;

    g_cb_calls = 0;
    g_cb_last = 0;
    s32 rc = CARDReadAsync(&fi, buf, 1024, 7680, cb_record);
    u32 h = fnv1a32(buf, 1024u);

    wr32be(out + w, 0x43525232u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, h); w += 4;
  }

  // Case C: misaligned length => fatal, no callback.
  {
    u8* dir = gc_ram_ptr(ram, 0x80414000u, CARD_SYSTEM_BLOCK_SIZE);
    u8* fat = gc_ram_ptr(ram, 0x80416000u, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir || !fat) die("gc_ram_ptr(dir/fat) failed");
    init_dir(dir);
    init_fat(fat);
    seed_entry(dir, (const u8*)"GAMEC0", (u8)PORT_CARD_ATTR_PUBLIC, 5, 2);
    fat_set(fat, 5, 6);
    fat_set(fat, 6, 0xFFFFu);
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    u8* buf = gc_ram_ptr(ram, 0x8041A000u, 512u);
    if (!buf) die("gc_ram_ptr(bufC) failed");
    memset(buf, 0x11u, 512u);
    u32 h_before = fnv1a32(buf, 512u);

    CARDFileInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.chan = 0;
    fi.fileNo = 0;
    fi.offset = 0;
    fi.iBlock = 5;

    g_cb_calls = 0;
    s32 rc = CARDReadAsync(&fi, buf, 1, 0, cb_record);
    u32 h_after = fnv1a32(buf, 512u);

    wr32be(out + w, 0x43525233u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, h_before); w += 4;
    wr32be(out + w, h_after); w += 4;
  }

  // Case D: NOPERM but PUBLIC => read allowed.
  {
    u8* dir = gc_ram_ptr(ram, 0x8041C000u, CARD_SYSTEM_BLOCK_SIZE);
    u8* fat = gc_ram_ptr(ram, 0x8041E000u, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir || !fat) die("gc_ram_ptr(dir/fat) failed");
    init_dir(dir);
    init_fat(fat);
    seed_entry(dir, (const u8*)"BAD000", (u8)PORT_CARD_ATTR_PUBLIC, 5, 2);
    fat_set(fat, 5, 6);
    fat_set(fat, 6, 0xFFFFu);
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    u8* buf = gc_ram_ptr(ram, 0x80424000u, 1024u);
    if (!buf) die("gc_ram_ptr(bufD) failed");
    memset(buf, 0x22u, 1024u);

    CARDFileInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.chan = 0;
    fi.fileNo = 0;
    fi.offset = 0;
    fi.iBlock = 5;

    g_cb_calls = 0;
    g_cb_last = 0;
    s32 rc = CARDReadAsync(&fi, buf, 1024, 0, cb_record);
    u32 h = fnv1a32(buf, 1024u);

    wr32be(out + w, 0x43525234u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, h); w += 4;
  }
}
