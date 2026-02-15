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
  for (u16 b = (u16)CARD_NUM_SYSTEM_BLOCK; b < cBlock; b++) fat_set(fat, b, PORT_CARD_FAT_AVAIL);
  fat_set(fat, (u16)PORT_CARD_FAT_CHECKCODE, checkCode);
  fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, (u16)(cBlock - (u16)CARD_NUM_SYSTEM_BLOCK));
  fat_set(fat, (u16)PORT_CARD_FAT_LASTSLOT, (u16)((u16)CARD_NUM_SYSTEM_BLOCK - 1u));
  u16 cs = 0, csi = 0;
  card_checksum_u16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
  fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUM, cs);
  fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUMINV, csi);
}

static void seed_entry(u8* dir, u32 fileNo, const u8* id6, const char* name, u16 startBlock, u16 lenBlocks)
{
  u8* ent = dir + fileNo * (u32)PORT_CARD_DIR_SIZE;
  memcpy(ent + PORT_CARD_DIR_OFF_GAMENAME, id6, 4);
  memcpy(ent + PORT_CARD_DIR_OFF_COMPANY, id6 + 4, 2);
  memset(ent + PORT_CARD_DIR_OFF_FILENAME, 0, PORT_CARD_FILENAME_MAX);
  for (u32 i = 0; i < (u32)PORT_CARD_FILENAME_MAX; i++) {
    ent[PORT_CARD_DIR_OFF_FILENAME + i] = (u8)name[i];
    if (name[i] == '\0') break;
  }
  wr16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK, startBlock);
  wr16be(ent + PORT_CARD_DIR_OFF_LENGTH, lenBlocks);
  wr32be(ent + PORT_CARD_DIR_OFF_TIME, 0u);
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result) { (void)chan; g_cb_calls++; g_cb_last = result; }

static void fill_memcard(int chan, u32 size)
{
  u8 tmp[512];
  for (u32 i = 0; i < size; i += (u32)sizeof(tmp)) {
    for (u32 j = 0; j < (u32)sizeof(tmp); j++) tmp[j] = (u8)((i + j) ^ 0xA5u);
    if (gc_memcard_write(chan, i, tmp, (u32)sizeof(tmp)) != 0) die("gc_memcard_write fill failed");
  }
}

static u32 hash_memcard_window(int chan, u32 off, u32 len)
{
  u8 buf[8192];
  if (len > sizeof(buf)) die("hash_memcard_window: len too big");
  if (gc_memcard_read(chan, off, buf, len) != 0) die("gc_memcard_read failed");
  return fnv1a32(buf, len);
}

const char* gc_scenario_label(void) { return "CARDWrite/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_write_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram)
{
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x200u);
  if (!out) die("gc_ram_ptr(out) failed");
  memset(out, 0, 0x200u);
  u32 w = 0;
  wr32be(out + w, 0x43575230u); w += 4; // CWR0

  // Configure memcard backing store.
  const char* mc_path = "../actual/card_write_pbt_001_memcard.raw";
  const u32 mc_size = 0x40000u;
  if (gc_memcard_insert(0, mc_path, mc_size) != 0) die("gc_memcard_insert failed");

  // Setup EXI model.
  EXIInit();
  gc_exi_probeex_ret[0] = 1;
  gc_exi_dma_hook = gc_memcard_exi_dma;

  // Case A: 1 sector write @0.
  {
    fill_memcard(0, mc_size);

    u8* dir = gc_ram_ptr(ram, 0x80400000u, CARD_SYSTEM_BLOCK_SIZE);
    u8* fat = gc_ram_ptr(ram, 0x80402000u, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir || !fat) die("gc_ram_ptr(dir/fat) failed");

    init_dir(dir, 0);
    init_fat(fat, 0, 64);
    seed_entry(dir, 0, (const u8*)"GAMEC0", "file", 5, 2);
    fat_set(fat, 5, 6);
    fat_set(fat, 6, 0xFFFFu);

    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].cblock = 64;
    memcpy(gc_card_block[0].id, "GAMEC0", 6);
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    u8* wrbuf = gc_ram_ptr(ram, 0x80404000u, 8192u);
    if (!wrbuf) die("gc_ram_ptr(wrbufA) failed");
    for (u32 i = 0; i < 8192u; i++) wrbuf[i] = (u8)(0x5Au ^ (u8)i);

    CARDFileInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.chan = 0;
    fi.fileNo = 0;
    fi.offset = 0;
    fi.iBlock = 5;

    g_cb_calls = 0;
    g_cb_last = 0;
    s32 rc = CARDWriteAsync(&fi, wrbuf, 8192, 0, cb_record);

    u32 h_sec0 = hash_memcard_window(0, 5u * 8192u, 8192u);
    u32 t = rd32be(dir + 0u * (u32)PORT_CARD_DIR_SIZE + PORT_CARD_DIR_OFF_TIME);
    u32 h_dir = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);

    wr32be(out + w, 0x43575231u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, t); w += 4;
    wr32be(out + w, h_sec0); w += 4;
    wr32be(out + w, h_dir); w += 4;
  }

  // Case B: 2 sector write @0.
  {
    fill_memcard(0, mc_size);

    u8* dir = gc_ram_ptr(ram, 0x80408000u, CARD_SYSTEM_BLOCK_SIZE);
    u8* fat = gc_ram_ptr(ram, 0x8040A000u, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir || !fat) die("gc_ram_ptr(dir/fat) failed");

    init_dir(dir, 0);
    init_fat(fat, 0, 64);
    seed_entry(dir, 0, (const u8*)"GAMEC0", "file", 5, 2);
    fat_set(fat, 5, 6);
    fat_set(fat, 6, 0xFFFFu);

    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].cblock = 64;
    memcpy(gc_card_block[0].id, "GAMEC0", 6);
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    u8* wrbuf = gc_ram_ptr(ram, 0x8040C000u, 16384u);
    if (!wrbuf) die("gc_ram_ptr(wrbufB) failed");
    for (u32 i = 0; i < 16384u; i++) wrbuf[i] = (u8)(0xA6u ^ (u8)i);

    CARDFileInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.chan = 0;
    fi.fileNo = 0;
    fi.offset = 0;
    fi.iBlock = 5;

    g_cb_calls = 0;
    g_cb_last = 0;
    s32 rc = CARDWriteAsync(&fi, wrbuf, 16384, 0, cb_record);

    u32 h_sec0 = hash_memcard_window(0, 5u * 8192u, 8192u);
    u32 h_sec1 = hash_memcard_window(0, 6u * 8192u, 8192u);
    u32 t = rd32be(dir + 0u * (u32)PORT_CARD_DIR_SIZE + PORT_CARD_DIR_OFF_TIME);
    u32 h_dir = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);

    wr32be(out + w, 0x43575232u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, t); w += 4;
    wr32be(out + w, h_sec0); w += 4;
    wr32be(out + w, h_sec1); w += 4;
    wr32be(out + w, h_dir); w += 4;
  }

  // Case C: misaligned length => fatal error, no write.
  {
    fill_memcard(0, mc_size);

    u8* dir = gc_ram_ptr(ram, 0x80410000u, CARD_SYSTEM_BLOCK_SIZE);
    u8* fat = gc_ram_ptr(ram, 0x80412000u, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir || !fat) die("gc_ram_ptr(dir/fat) failed");

    init_dir(dir, 0);
    init_fat(fat, 0, 64);
    seed_entry(dir, 0, (const u8*)"GAMEC0", "file", 5, 2);
    fat_set(fat, 5, 6);
    fat_set(fat, 6, 0xFFFFu);

    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].cblock = 64;
    memcpy(gc_card_block[0].id, "GAMEC0", 6);
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    u8* wrbuf = gc_ram_ptr(ram, 0x80414000u, 8192u);
    if (!wrbuf) die("gc_ram_ptr(wrbufC) failed");
    memset(wrbuf, 0x11, 8192u);

    u32 h_before = hash_memcard_window(0, 5u * 8192u, 8192u);

    CARDFileInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.chan = 0;
    fi.fileNo = 0;
    fi.iBlock = 5;

    g_cb_calls = 0;
    s32 rc = CARDWriteAsync(&fi, wrbuf, 1, 0, cb_record);
    u32 h_after = hash_memcard_window(0, 5u * 8192u, 8192u);
    u32 h_dir = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);

    wr32be(out + w, 0x43575233u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, h_before); w += 4;
    wr32be(out + w, h_after); w += 4;
    wr32be(out + w, h_dir); w += 4;
  }

  // Case D: NOPERM (wrong game/company), no write.
  {
    fill_memcard(0, mc_size);

    u8* dir = gc_ram_ptr(ram, 0x80418000u, CARD_SYSTEM_BLOCK_SIZE);
    u8* fat = gc_ram_ptr(ram, 0x8041A000u, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir || !fat) die("gc_ram_ptr(dir/fat) failed");

    init_dir(dir, 0);
    init_fat(fat, 0, 64);
    seed_entry(dir, 0, (const u8*)"BAD000", "file", 5, 2);
    fat_set(fat, 5, 6);
    fat_set(fat, 6, 0xFFFFu);

    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].cblock = 64;
    memcpy(gc_card_block[0].id, "GAMEC0", 6);
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    u8* wrbuf = gc_ram_ptr(ram, 0x8041C000u, 8192u);
    if (!wrbuf) die("gc_ram_ptr(wrbufD) failed");
    memset(wrbuf, 0x22, 8192u);

    u32 h_before = hash_memcard_window(0, 5u * 8192u, 8192u);

    CARDFileInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.chan = 0;
    fi.fileNo = 0;
    fi.iBlock = 5;

    g_cb_calls = 0;
    s32 rc = CARDWriteAsync(&fi, wrbuf, 8192, 0, cb_record);
    u32 h_after = hash_memcard_window(0, 5u * 8192u, 8192u);
    u32 h_dir = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);

    wr32be(out + w, 0x43575234u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, h_before); w += 4;
    wr32be(out + w, h_after); w += 4;
    wr32be(out + w, h_dir); w += 4;
  }
}
