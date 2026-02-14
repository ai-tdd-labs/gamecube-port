#include <stdint.h>
#include <string.h>

#include "src/sdk_port/card/card_bios.h"
#include "src/sdk_port/card/card_dir.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_NOFILE = -4,
  CARD_RESULT_NOCARD = -3,
  CARD_RESULT_BROKEN = -6,
  CARD_RESULT_FATAL_ERROR = -128,
};

GcCardControl gc_card_block[GC_CARD_CHANS];

static int os_disable_count;
static int os_restore_count;

static int os_disable_interrupts(void) {
  return os_disable_count++;
}

static void os_restore_interrupts(int enabled) {
  (void)enabled;
  os_restore_count++;
}

// Minimal __CARDGetControlBlock/__CARDPutControlBlock for oracle-only execution.
s32 __CARDGetControlBlock(int32_t chan, GcCardControl** pcard) {
  GcCardControl* card;
  s32 result;

  if (chan < 0 || chan >= GC_CARD_CHANS) {
    return CARD_RESULT_FATAL_ERROR;
  }
  card = &gc_card_block[chan];
  if (!card->disk_id) {
    return CARD_RESULT_FATAL_ERROR;
  }

  os_disable_interrupts();
  if (!card->attached) {
    result = CARD_RESULT_NOCARD;
  } else if (card->result == CARD_RESULT_BUSY) {
    result = CARD_RESULT_BUSY;
  } else {
    card->result = CARD_RESULT_BUSY;
    if (pcard) {
      *pcard = card;
    }
    result = CARD_RESULT_READY;
  }
  os_restore_interrupts(0);
  return result;
}

s32 __CARDPutControlBlock(GcCardControl* card, s32 result) {
  if (!card) {
    return result;
  }
  if (card->attached || card->result == CARD_RESULT_BUSY) {
    card->result = result;
  }
  return result;
}

static inline void wr32be(u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}

static inline u16 be16(const u8* p) {
  return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static inline s32 is_valid_block_no(port_CARDDirControl* card, u16 iBlock) {
  return (PORT_CARD_DIR_NUM_SYSTEM_BLOCK <= iBlock && iBlock < (s32)card->cBlock);
}

static int card_compare_file_name(u32 dir_entry_addr, const char* file_name) {
  const u8* p = (const u8*)dir_entry_addr + PORT_CARD_DIR_OFF_FILENAME;
  int n = PORT_CARD_FILENAME_MAX;
  for (;;) {
    int i = --n;
    if (i < 0) {
      return *file_name == '\0';
    }
    if ((char)p[0] != *file_name++) {
      return 0;
    }
    if (p[0] == '\0') {
      return 1;
    }
    p++;
  }
}

static s32 card_access(port_CARDDirControl* card, u32 dir_entry_addr) {
  if (((const u8*)dir_entry_addr)[PORT_CARD_DIR_OFF_GAMENAME] == 0xFFu) {
    return CARD_RESULT_NOFILE;
  }
  return CARD_RESULT_READY;
}

static s32 card_get_file_no(port_CARDDirControl* card, const char* file_name,
                            s32* p_file_no) {
  if (!card->attached) {
    return CARD_RESULT_NOCARD;
  }
  for (s32 file_no = 0; file_no < PORT_CARD_MAX_FILE; file_no++) {
    u32 ent = card->dir_addr + (u32)file_no * PORT_CARD_DIR_SIZE;
    s32 rc = card_access(card, ent);
    if (rc < 0) {
      continue;
    }
    if (card_compare_file_name(ent, file_name)) {
      *p_file_no = file_no;
      return CARD_RESULT_READY;
    }
  }
  return CARD_RESULT_NOFILE;
}

s32 oracle_CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo) {
  GcCardControl* card;
  port_CARDDirControl view;
  s32 result;
  s32 file_no;
  const u8* dir;
  u16 start_block;

  fileInfo->chan = -1;
  result = __CARDGetControlBlock(chan, &card);
  if (result < 0) {
    return result;
  }

  memset(&view, 0, sizeof(view));
  view.attached = card->attached;
  view.cBlock = (u16)card->cblock;
  view.sectorSize = card->sector_size;
  view.dir_addr = (u32)card->current_dir_ptr;
  view.fat_addr = (u32)card->current_fat_ptr;
  view.diskID_is_none = 1;

  result = card_get_file_no(&view, fileName, &file_no);
  if (0 <= result) {
    dir = (const u8*)card->current_dir_ptr;
    start_block = be16(dir + (u32)file_no * PORT_CARD_DIR_SIZE +
                       PORT_CARD_DIR_OFF_STARTBLOCK);
    if (!is_valid_block_no(&view, start_block)) {
      result = CARD_RESULT_BROKEN;
    } else {
      fileInfo->chan = chan;
      fileInfo->fileNo = file_no;
      fileInfo->offset = 0;
      fileInfo->iBlock = start_block;
    }
  }

  return __CARDPutControlBlock(card, result);
}

s32 oracle_CARDClose(CARDFileInfo* fileInfo) {
  GcCardControl* card;
  s32 result;

  result = __CARDGetControlBlock(fileInfo->chan, &card);
  if (result < 0) {
    return result;
  }

  fileInfo->chan = -1;
  return __CARDPutControlBlock(card, CARD_RESULT_READY);
}

static inline void init_entry(u8* ent, const char* name, u16 start_block) {
  for (u32 i = 0; i < PORT_CARD_DIR_SIZE; i++) {
    ent[i] = 0xFFu;
  }
  ent[PORT_CARD_DIR_OFF_GAMENAME] = 0x00u;
  ent[PORT_CARD_DIR_OFF_COMPANY] = 0x00u;
  for (u32 i = 0; i < PORT_CARD_FILENAME_MAX; i++) {
    ent[PORT_CARD_DIR_OFF_FILENAME + i] = (u8)name[i];
    if (name[i] == '\0') {
      break;
    }
  }
  ent[PORT_CARD_DIR_OFF_STARTBLOCK] = (u8)(start_block >> 8);
  ent[PORT_CARD_DIR_OFF_STARTBLOCK + 1] = (u8)start_block;
}

static void dump_case(u8* out, u32* w, u32 case_id, s32 ret,
                      const CARDFileInfo* fi) {
  wr32be(out + ((*w)++ * 4u), case_id);
  wr32be(out + ((*w)++ * 4u), (u32)ret);
  wr32be(out + ((*w)++ * 4u), (u32)fi->chan);
  wr32be(out + ((*w)++ * 4u), (u32)fi->fileNo);
  wr32be(out + ((*w)++ * 4u), (u32)fi->offset);
  wr32be(out + ((*w)++ * 4u), (u32)fi->iBlock);
  wr32be(out + ((*w)++ * 4u), (u32)gc_card_block[0].result);
}

static void poison_file_info(CARDFileInfo* fi) {
  memset(fi, 0x55u, sizeof(*fi));
}

void oracle_card_open_suite(u8* out) {
  static u8 dir[0x2000];
  static u8 fat[0x2000];
  CARDFileInfo fi = {0};

  memset(gc_card_block, 0, sizeof(gc_card_block));
  memset(dir, 0, sizeof(dir));
  memset(fat, 0, sizeof(fat));
  gc_card_block[0].disk_id = 0x80000000u;
  gc_card_block[0].attached = 1;
  gc_card_block[0].result = CARD_RESULT_READY;
  gc_card_block[0].sector_size = 8192;
  gc_card_block[0].cblock = 64;
  gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
  gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

  init_entry(dir + 0u * PORT_CARD_DIR_SIZE, "savefile.bin", 5u);
  init_entry(dir + 1u * PORT_CARD_DIR_SIZE, "badstart.bin", 3u);

  wr32be(out + 0u * 4u, 0x434f5030u); // "COP0"
  wr32be(out + 1u * 4u, 7u);
  u32 w = 2u;

  poison_file_info(&fi);
  s32 rc0 = oracle_CARDOpen(0, "savefile.bin", &fi);
  dump_case(out, &w, 0u, rc0, &fi);

  poison_file_info(&fi);
  s32 rc1 = oracle_CARDOpen(0, "missing.bin", &fi);
  dump_case(out, &w, 1u, rc1, &fi);

  poison_file_info(&fi);
  s32 rc2 = oracle_CARDOpen(0, "badstart.bin", &fi);
  dump_case(out, &w, 2u, rc2, &fi);

  poison_file_info(&fi);
  s32 rc3 = oracle_CARDOpen(-1, "savefile.bin", &fi);
  dump_case(out, &w, 3u, rc3, &fi);

  gc_card_block[0].attached = 0;
  poison_file_info(&fi);
  s32 rc4 = oracle_CARDOpen(0, "savefile.bin", &fi);
  dump_case(out, &w, 4u, rc4, &fi);

  gc_card_block[0].attached = 1;
  gc_card_block[0].result = CARD_RESULT_READY;
  poison_file_info(&fi);
  s32 rc5_open = oracle_CARDOpen(0, "savefile.bin", &fi);
  (void)rc5_open;
  s32 rc5 = oracle_CARDClose(&fi);
  dump_case(out, &w, 5u, rc5, &fi);

  fi.chan = -1;
  s32 rc6 = oracle_CARDClose(&fi);
  dump_case(out, &w, 6u, rc6, &fi);
}
