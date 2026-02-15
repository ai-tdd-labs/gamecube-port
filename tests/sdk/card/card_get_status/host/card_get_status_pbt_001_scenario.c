#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_port/card/card_bios.h"
#include "sdk_port/card/card_dir.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_NOFILE = -4,
  CARD_RESULT_NOPERM = -10,
  CARD_RESULT_FATAL_ERROR = -128,
};

enum {
  CARD_GET_STATUS_DIR_ADDR = 0x80400000u,
};

s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat);

static inline u32 pack4(const u8* p) {
  return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static inline u32 mem_addr(u8* base, u32 base_addr, const void* p) {
  return (u32)((uintptr_t)p - (uintptr_t)base + base_addr);
}

static void reset_card_state(void) {
  memset(gc_card_block, 0, sizeof(gc_card_block));
  gc_card_block[0].disk_id = 0x80000000u;
  gc_card_block[0].attached = 1;
  gc_card_block[0].result = CARD_RESULT_READY;
  gc_card_block[0].cblock = 64;
  gc_card_block[0].sector_size = 8192;
}

static void write_dir_entry(u8* ent, const char* file_name, const char* game_name, const char* company,
                           u16 length, u32 time, u32 icon_addr, u16 icon_format,
                           u8 banner_format, u16 icon_speed, u32 comment_addr) {
  memset(ent, 0xFFu, PORT_CARD_DIR_SIZE);
  for (u32 i = 0; i < 4u; i++) {
    ent[PORT_CARD_DIR_OFF_GAMENAME + i] = (u8)game_name[i];
    ent[PORT_CARD_DIR_OFF_COMPANY + i] = (i < 2u) ? (u8)company[i] : 0u;
  }
  for (u32 i = 0; i < PORT_CARD_FILENAME_MAX; i++) {
    ent[PORT_CARD_DIR_OFF_FILENAME + i] = (u8)file_name[i];
    if (file_name[i] == '\0') {
      break;
    }
  }

  ent[PORT_CARD_DIR_OFF_STARTBLOCK] = 0x00u;
  ent[PORT_CARD_DIR_OFF_STARTBLOCK + 1u] = 0x05u;
  ent[PORT_CARD_DIR_OFF_LENGTH] = (u8)(length >> 8);
  ent[PORT_CARD_DIR_OFF_LENGTH + 1u] = (u8)length;

  ent[PORT_CARD_DIR_OFF_TIME] = (u8)(time >> 24);
  ent[PORT_CARD_DIR_OFF_TIME + 1u] = (u8)(time >> 16);
  ent[PORT_CARD_DIR_OFF_TIME + 2u] = (u8)(time >> 8);
  ent[PORT_CARD_DIR_OFF_TIME + 3u] = (u8)time;

  ent[PORT_CARD_DIR_OFF_BANNER_FORMAT] = banner_format;
  ent[PORT_CARD_DIR_OFF_ICON_ADDR] = (u8)(icon_addr >> 24);
  ent[PORT_CARD_DIR_OFF_ICON_ADDR + 1u] = (u8)(icon_addr >> 16);
  ent[PORT_CARD_DIR_OFF_ICON_ADDR + 2u] = (u8)(icon_addr >> 8);
  ent[PORT_CARD_DIR_OFF_ICON_ADDR + 3u] = (u8)icon_addr;
  ent[PORT_CARD_DIR_OFF_ICON_FORMAT] = (u8)(icon_format >> 8);
  ent[PORT_CARD_DIR_OFF_ICON_FORMAT + 1u] = (u8)icon_format;
  ent[PORT_CARD_DIR_OFF_ICON_SPEED] = (u8)(icon_speed >> 8);
  ent[PORT_CARD_DIR_OFF_ICON_SPEED + 1u] = (u8)icon_speed;
  ent[PORT_CARD_DIR_OFF_COMMENT_ADDR] = (u8)(comment_addr >> 24);
  ent[PORT_CARD_DIR_OFF_COMMENT_ADDR + 1u] = (u8)(comment_addr >> 16);
  ent[PORT_CARD_DIR_OFF_COMMENT_ADDR + 2u] = (u8)(comment_addr >> 8);
  ent[PORT_CARD_DIR_OFF_COMMENT_ADDR + 3u] = (u8)comment_addr;
}

static void dump_case(u8* out, u32* w, u32 tag, s32 rc, const CARDStat* stat) {
  wr32be(out + (*w * 4u), tag);
  (*w)++;
  wr32be(out + (*w * 4u), (u32)rc);
  (*w)++;
  wr32be(out + (*w * 4u), (u32)gc_card_block[0].result);
  (*w)++;
  wr32be(out + (*w * 4u), stat->length);
  (*w)++;
  wr32be(out + (*w * 4u), stat->time);
  (*w)++;
    wr32be(out + (*w * 4u), pack4((const u8*)stat->fileName + 0u));
  (*w)++;
    wr32be(out + (*w * 4u), pack4((const u8*)stat->fileName + 4u));
  (*w)++;
    wr32be(out + (*w * 4u), pack4((const u8*)stat->gameName + 0u));
  (*w)++;
  wr32be(out + (*w * 4u), ((u32)(u8)stat->company[0] << 24) | ((u32)(u8)stat->company[1] << 16));
  (*w)++;
  wr32be(out + (*w * 4u), (u32)stat->bannerFormat);
  (*w)++;
  wr32be(out + (*w * 4u), stat->iconAddr);
  (*w)++;
  wr32be(out + (*w * 4u), (u32)stat->iconFormat);
  (*w)++;
  wr32be(out + (*w * 4u), (u32)stat->iconSpeed);
  (*w)++;
  wr32be(out + (*w * 4u), stat->commentAddr);
  (*w)++;
  wr32be(out + (*w * 4u), stat->offsetBanner);
  (*w)++;
  wr32be(out + (*w * 4u), stat->offsetBannerTlut);
  (*w)++;
  for (u32 i = 0; i < CARD_ICON_MAX; i++) {
    wr32be(out + (*w * 4u), stat->offsetIcon[i]);
    (*w)++;
  }
  wr32be(out + (*w * 4u), stat->offsetIconTlut);
  (*w)++;
  wr32be(out + (*w * 4u), stat->offsetData);
  (*w)++;
}

const char* gc_scenario_label(void) {
  return "CARDGetStatus/pbt_001";
}

const char* gc_scenario_out_path(void) {
  return "../actual/card_get_status_pbt_001.bin";
}

void gc_scenario_run(GcRam* ram) {
  u8* out = gc_ram_ptr(ram, 0x80300000u, 0x400u);
  if (!out) {
    die("gc_ram_ptr(out) failed");
  }
  memset(out, 0, 0x400u);

  u8* dir = gc_ram_ptr(ram, CARD_GET_STATUS_DIR_ADDR, 0x3000u);
  if (!dir) {
    die("gc_ram_ptr(dir) failed");
  }
  memset(dir, 0xFFu, 0x3000u);

  reset_card_state();
  gc_card_block[0].current_dir_ptr = mem_addr(dir, CARD_GET_STATUS_DIR_ADDR, dir);
  gc_card_block[0].current_fat_ptr = 0u;

  write_dir_entry(
    dir + 0u * PORT_CARD_DIR_SIZE,
    "slot00",
    "GAME",
    "C0",
    4u,
    0xA0A1A2A3u,
    0x00000200u,
    (u16)((CARD_STAT_ICON_C8 << 0) | (CARD_STAT_ICON_RGB5A3 << 2)),
    CARD_STAT_BANNER_C8,
    (u16)((CARD_STAT_SPEED_FAST << 0) | (CARD_STAT_SPEED_MIDDLE << 2)),
    0x00001000u
  );
  write_dir_entry(
    dir + 2u * PORT_CARD_DIR_SIZE,
    "slot02",
    "GAME",
    "C0",
    3u,
    0xB0B1B2B3u,
    0xFFFFFFFFu,
    CARD_STAT_ICON_NONE,
    CARD_STAT_BANNER_NONE,
    0x00000000u,
    0x00002000u
  );
  write_dir_entry(
    dir + 3u * PORT_CARD_DIR_SIZE,
    "slot03",
    "GAME",
    "C0",
    2u,
    0xC0C1C2C3u,
    0x00000400u,
    CARD_STAT_ICON_NONE,
    CARD_STAT_BANNER_RGB5A3,
    0x00000000u,
    0x00003000u
  );

  CARDStat stat;
  u32 w = 2u;

  wr32be(out + 0u, 0x43475330u);
  wr32be(out + 4u, 0x00000000u);

  memset(&stat, 0xE5u, sizeof(stat));
  s32 rc = CARDGetStatus(0, 0, &stat);
  dump_case(out, &w, 0x43475331u, rc, &stat);

  memset(&stat, 0xE5u, sizeof(stat));
  rc = CARDGetStatus(0, PORT_CARD_MAX_FILE, &stat);
  dump_case(out, &w, 0x43475332u, rc, &stat);

  memset(&stat, 0xE5u, sizeof(stat));
  rc = CARDGetStatus(-1, 0, &stat);
  dump_case(out, &w, 0x43475333u, rc, &stat);

  memset(&stat, 0xE5u, sizeof(stat));
  gc_card_block[0].attached = 0;
  rc = CARDGetStatus(0, 0, &stat);
  gc_card_block[0].attached = 1;
  dump_case(out, &w, 0x43475334u, rc, &stat);

  memset(&stat, 0xE5u, sizeof(stat));
  gc_card_block[0].result = CARD_RESULT_BUSY;
  rc = CARDGetStatus(0, 0, &stat);
  gc_card_block[0].result = CARD_RESULT_READY;
  dump_case(out, &w, 0x43475335u, rc, &stat);

  memset(&stat, 0xE5u, sizeof(stat));
  rc = CARDGetStatus(0, 1, &stat);
  dump_case(out, &w, 0x43475336u, rc, &stat);

  memset(&stat, 0xE5u, sizeof(stat));
  rc = CARDGetStatus(0, 2, &stat);
  dump_case(out, &w, 0x43475337u, rc, &stat);

  memset(&stat, 0xE5u, sizeof(stat));
  rc = CARDGetStatus(0, 3, &stat);
  dump_case(out, &w, 0x43475338u, rc, &stat);
}
