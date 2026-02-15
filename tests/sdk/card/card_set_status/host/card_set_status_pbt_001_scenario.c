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
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_NOPERM = -10,
    CARD_RESULT_FATAL_ERROR = -128,

    CARD_SET_STATUS_DIR_ADDR = 0x80400000u,
};

s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat* stat);
s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat* stat, CARDCallback callback);

static inline u8* mem_ptr(u8* dir_base, u32 gc_addr)
{
    return dir_base + (gc_addr - CARD_SET_STATUS_DIR_ADDR);
}

static volatile s32 g_async_result = 0x11111111;

static void set_status_callback(s32 chan, s32 rc) {
    (void)chan;
    g_async_result = rc;
}

static inline u32 load_u32be(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static inline u16 load_u16be(const u8* p) {
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static void reset_card_state(void) {
    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].cblock = 64;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].current_dir_ptr = 0u;
    gc_card_block[0].current_fat_ptr = 0u;
}

static void write_dir_entry(u8* ent, const char* file_name, const char* game_name, const char* company,
                           u16 length, u32 icon_addr, u16 icon_format,
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
    ent[PORT_CARD_DIR_OFF_PERMISSION] = PORT_CARD_ATTR_PUBLIC;
}

static void dump_dir_entry(u8* out, u32* w, u8* ent) {
    wr32be(out + (*w * 4u), load_u32be(ent + PORT_CARD_DIR_OFF_BANNER_FORMAT));
    (*w)++;
    wr32be(out + (*w * 4u), load_u32be(ent + PORT_CARD_DIR_OFF_ICON_ADDR));
    (*w)++;
    wr32be(out + (*w * 4u), (u32)load_u16be(ent + PORT_CARD_DIR_OFF_ICON_FORMAT));
    (*w)++;
    wr32be(out + (*w * 4u), (u32)load_u16be(ent + PORT_CARD_DIR_OFF_ICON_SPEED));
    (*w)++;
    wr32be(out + (*w * 4u), load_u32be(ent + PORT_CARD_DIR_OFF_COMMENT_ADDR));
    (*w)++;
    wr32be(out + (*w * 4u), load_u32be(ent + PORT_CARD_DIR_OFF_TIME));
    (*w)++;
}

static void dump_stat(u8* out, u32* w, const CARDStat* stat) {
    wr32be(out + (*w * 4u), stat->bannerFormat);
    (*w)++;
    wr32be(out + (*w * 4u), stat->iconAddr);
    (*w)++;
    wr32be(out + (*w * 4u), (u32)stat->iconFormat);
    (*w)++;
    wr32be(out + (*w * 4u), (u32)stat->iconSpeed);
    (*w)++;
    wr32be(out + (*w * 4u), stat->commentAddr);
    (*w)++;
    wr32be(out + (*w * 4u), stat->time);
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

static void dump_case(u8* out, u32* w, u32 tag, s32 rc, u8* dir_base, u32 file_no, const CARDStat* stat) {
    u8* entry = mem_ptr(dir_base, (u32)(CARD_SET_STATUS_DIR_ADDR + (u32)file_no * PORT_CARD_DIR_SIZE));

    wr32be(out + (*w * 4u), tag);
    (*w)++;
    wr32be(out + (*w * 4u), (u32)rc);
    (*w)++;
    wr32be(out + (*w * 4u), (u32)gc_card_block[0].result);
    (*w)++;

    dump_dir_entry(out, w, entry);
    dump_stat(out, w, stat);
}

const char* gc_scenario_label(void) {
    return "CARDSetStatus/pbt_001";
}

const char* gc_scenario_out_path(void) {
    return "../actual/card_set_status_pbt_001.bin";
}

void gc_scenario_run(GcRam* ram) {
    u8* out = gc_ram_ptr(ram, 0x80300000u, 0x400u);
    if (!out) {
        die("gc_ram_ptr(out) failed");
    }
    memset(out, 0, 0x400u);

    u8* dir = gc_ram_ptr(ram, CARD_SET_STATUS_DIR_ADDR, 0x3000u);
    if (!dir) {
        die("gc_ram_ptr(dir) failed");
    }
    memset(dir, 0xFFu, 0x3000u);

    reset_card_state();
    gc_card_block[0].current_dir_ptr = CARD_SET_STATUS_DIR_ADDR;
    gc_card_block[0].current_fat_ptr = CARD_SET_STATUS_DIR_ADDR;

    write_dir_entry(
        dir + 0u * PORT_CARD_DIR_SIZE,
        "slot00",
        "GAME",
        "C0",
        4u,
        0x00000200u,
        (u16)((CARD_STAT_ICON_C8 << 0) | (CARD_STAT_ICON_RGB5A3 << 2)),
        CARD_STAT_BANNER_C8,
        (u16)((CARD_STAT_SPEED_FAST << 0) | (CARD_STAT_SPEED_MIDDLE << 2)),
        0x00001000u
    );
    write_dir_entry(
        dir + 1u * PORT_CARD_DIR_SIZE,
        "slot01",
        "GAME",
        "C0",
        1u,
        0xFFFFFFFFu,
        CARD_STAT_ICON_NONE,
        CARD_STAT_BANNER_NONE,
        0x00000000u,
        0x00002000u
    );

    CARDStat stat;
    u32 w = 2u;
    wr32be(out + 0u, 0x434E5341u);
    wr32be(out + 4u, 0x00000000u);

    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = 0x00000400u;
    stat.iconFormat = (u16)(CARD_STAT_ICON_RGB5A3 << 0);
    stat.iconSpeed = (u16)CARD_STAT_SPEED_SLOW;
    stat.commentAddr = 0x00003000u;
    s32 rc = CARDSetStatus(0, 0, &stat);
    dump_case(out, &w, 0x434E534Bu, rc, dir, 0, &stat);

    rc = CARDSetStatus(0, PORT_CARD_MAX_FILE, &stat);
    memset(&stat, 0xE5u, sizeof(stat));
    dump_case(out, &w, 0x434E5342u, rc, dir, 0, &stat);

    rc = CARDSetStatus(-1, 0, &stat);
    memset(&stat, 0xE5u, sizeof(stat));
    dump_case(out, &w, 0x434E5343u, rc, dir, 0, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = CARD_READ_SIZE;
    stat.iconFormat = (u16)(CARD_STAT_ICON_C8 << 0);
    stat.iconSpeed = (u16)CARD_STAT_SPEED_FAST;
    stat.commentAddr = 0x00004000u;
    rc = CARDSetStatus(0, 0, &stat);
    dump_case(out, &w, 0x434E5344u, rc, dir, 0, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = 0x00000400u;
    stat.iconFormat = (u16)(CARD_STAT_ICON_RGB5A3 << 0);
    stat.iconSpeed = (u16)CARD_STAT_SPEED_MIDDLE;
    stat.commentAddr = 0x1FE9u;
    rc = CARDSetStatus(0, 0, &stat);
    dump_case(out, &w, 0x434E5345u, rc, dir, 0, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = 0x00000400u;
    stat.iconFormat = (u16)(CARD_STAT_ICON_C8 << 0);
    stat.iconSpeed = (u16)CARD_STAT_SPEED_MIDDLE;
    stat.commentAddr = 0x00003000u;
    gc_card_block[0].attached = 0;
    rc = CARDSetStatus(0, 0, &stat);
    gc_card_block[0].attached = 1;
    dump_case(out, &w, 0x434E5346u, rc, dir, 0, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = 0xDEADBEEFu;
    stat.iconAddr = 0xFFFFFFFFu;
    stat.iconFormat = (u16)0xBADC;
    stat.iconSpeed = 0x1111u;
    stat.commentAddr = 0x00005000u;
    rc = CARDSetStatus(0, 1, &stat);
    dump_case(out, &w, 0x434E5347u, rc, dir, 1, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    g_async_result = 0x11111111u;
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = 0x00000400u;
    stat.iconFormat = (u16)(CARD_STAT_ICON_C8 << 0);
    stat.iconSpeed = (u16)CARD_STAT_SPEED_MIDDLE;
    stat.commentAddr = 0x00003000u;
    rc = CARDSetStatusAsync(0, 0, &stat, set_status_callback);
    wr32be(out + (w * 4u), 0x434E534Au);
    w++;
    wr32be(out + (w * 4u), (u32)rc);
    w++;
    wr32be(out + (w * 4u), (u32)g_async_result);
    w++;
    wr32be(out + (w * 4u), (u32)gc_card_block[0].result);
    w++;
    dump_stat(out, &w, &stat);

    memset(dir + 1u * PORT_CARD_DIR_SIZE, 0xFFu, PORT_CARD_DIR_SIZE);
    rc = CARDSetStatus(0, 1, &stat);
    dump_case(out, &w, 0x434E5348u, rc, dir, 1, &stat);
}
