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
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_NOFILE = -4,
    CARD_RESULT_NOPERM = -10,
    CARD_RESULT_FATAL_ERROR = -128,
};

enum {
    PORT_CARD_DIR_OFF_BANNER_FORMAT = 7,
    PORT_CARD_DIR_OFF_TIME = 40,
    PORT_CARD_DIR_OFF_ICON_ADDR = 44,
    PORT_CARD_DIR_OFF_ICON_FORMAT = 48,
    PORT_CARD_DIR_OFF_ICON_SPEED = 50,
    PORT_CARD_DIR_OFF_COMMENT_ADDR = 60,
};

GcCardControl gc_card_block[GC_CARD_CHANS];

s32 __CARDGetControlBlock(s32 chan, GcCardControl **pcard)
{
    GcCardControl *card;

    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return CARD_RESULT_FATAL_ERROR;
    }
    card = &gc_card_block[chan];
    if (!card->disk_id) {
        return CARD_RESULT_FATAL_ERROR;
    }
    if (!card->attached) {
        return CARD_RESULT_NOCARD;
    }
    if (card->result == CARD_RESULT_BUSY) {
        return CARD_RESULT_BUSY;
    }

    card->result = CARD_RESULT_BUSY;
    card->api_callback = 0;
    if (pcard) {
        *pcard = card;
    }
    return CARD_RESULT_READY;
}

s32 __CARDPutControlBlock(GcCardControl *card, s32 result)
{
    if (!card) {
        return result;
    }
    if (card->attached || card->result == CARD_RESULT_BUSY) {
        card->result = result;
    }
    return result;
}

static inline u8 load_u8(uint32_t addr)
{
    u8 *p = (u8*)addr;
    return p[0];
}

static inline u16 load_u16be(uint32_t addr)
{
    u8 *p = (u8*)addr;
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static inline u32 load_u32be(uint32_t addr)
{
    u8 *p = (u8*)addr;
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

s32 port_CARDAccess(port_CARDDirControl *card, uint32_t dir_entry_addr)
{
    (void)card;
    if (load_u8(dir_entry_addr + PORT_CARD_DIR_OFF_GAMENAME) == 0xFFu) {
        return CARD_RESULT_NOFILE;
    }
    return CARD_RESULT_READY;
}

s32 port_CARDIsPublic(uint32_t dir_entry_addr)
{
    if (load_u8(dir_entry_addr + PORT_CARD_DIR_OFF_GAMENAME) == 0xFFu) {
        return CARD_RESULT_NOFILE;
    }
    if ((load_u8(dir_entry_addr + PORT_CARD_DIR_OFF_PERMISSION) & PORT_CARD_ATTR_PUBLIC) != 0) {
        return CARD_RESULT_READY;
    }
    return CARD_RESULT_NOPERM;
}

static inline s32 card_get_banner_format(u32 ent_addr)
{
    return (s32)(load_u8(ent_addr + PORT_CARD_DIR_OFF_BANNER_FORMAT) & CARD_STAT_BANNER_MASK);
}

static inline s32 card_get_icon_format(u32 ent_addr, s32 icon_no)
{
    if (icon_no < 0 || CARD_ICON_MAX <= icon_no) {
        return CARD_STAT_ICON_NONE;
    }
    u16 fmt = load_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_FORMAT);
    return (s32)((fmt >> (icon_no * 2)) & CARD_STAT_ICON_MASK);
}

static void update_icon_offsets(u32 ent_addr, CARDStat *stat)
{
    u32 offset;
    u32 iconTlut = 0u;

    offset = load_u32be(ent_addr + PORT_CARD_DIR_OFF_ICON_ADDR);
    if (offset == 0xFFFFFFFFu) {
        stat->bannerFormat = CARD_STAT_BANNER_NONE;
        stat->iconFormat = CARD_STAT_ICON_NONE;
        stat->iconSpeed = 0;
        offset = 0;
    }

    switch (card_get_banner_format(ent_addr)) {
        case CARD_STAT_BANNER_C8:
            stat->offsetBanner = offset;
            offset += CARD_BANNER_WIDTH * CARD_BANNER_HEIGHT;
            stat->offsetBannerTlut = offset;
            offset += 2u * 256u;
            break;
        case CARD_STAT_BANNER_RGB5A3:
            stat->offsetBanner = offset;
            offset += 2u * CARD_BANNER_WIDTH * CARD_BANNER_HEIGHT;
            stat->offsetBannerTlut = 0xFFFFFFFFu;
            break;
        default:
            stat->offsetBanner = 0xFFFFFFFFu;
            stat->offsetBannerTlut = 0xFFFFFFFFu;
            break;
    }

    for (u32 i = 0; i < CARD_ICON_MAX; i++) {
        switch (card_get_icon_format(ent_addr, (s32)i)) {
            case CARD_STAT_ICON_C8:
                stat->offsetIcon[i] = offset;
                offset += CARD_ICON_WIDTH * CARD_ICON_HEIGHT;
                iconTlut = 1;
                break;
            case CARD_STAT_ICON_RGB5A3:
                stat->offsetIcon[i] = offset;
                offset += 2u * CARD_ICON_WIDTH * CARD_ICON_HEIGHT;
                break;
            default:
                stat->offsetIcon[i] = 0xFFFFFFFFu;
                break;
        }
    }

    if (iconTlut) {
        stat->offsetIconTlut = offset;
        offset += 2u * 256u;
    } else {
        stat->offsetIconTlut = 0xFFFFFFFFu;
    }

    stat->offsetData = offset;
}

s32 oracle_CARDGetStatus(s32 chan, s32 fileNo, CARDStat *stat)
{
    GcCardControl* card;
    s32 result;
    u32 dir_addr;
    port_CARDDirControl view;

    if (fileNo < 0 || PORT_CARD_MAX_FILE <= fileNo) {
        return CARD_RESULT_FATAL_ERROR;
    }

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

    dir_addr = (u32)card->current_dir_ptr + (u32)fileNo * PORT_CARD_DIR_SIZE;
    result = port_CARDAccess(&view, dir_addr);
    if (result == CARD_RESULT_NOPERM) {
        result = port_CARDIsPublic(dir_addr);
    }

    if (result >= 0) {
        memcpy(stat->gameName, (void*)(dir_addr + PORT_CARD_DIR_OFF_GAMENAME), sizeof(stat->gameName));
        memcpy(stat->company, (void*)(dir_addr + PORT_CARD_DIR_OFF_COMPANY), sizeof(stat->company));
        stat->length = (u32)load_u16be(dir_addr + PORT_CARD_DIR_OFF_LENGTH) * (u32)card->sector_size;
        memcpy(stat->fileName, (void*)(dir_addr + PORT_CARD_DIR_OFF_FILENAME), CARD_FILENAME_MAX);
        stat->time = load_u32be(dir_addr + PORT_CARD_DIR_OFF_TIME);

        stat->bannerFormat = load_u8(dir_addr + PORT_CARD_DIR_OFF_BANNER_FORMAT);
        stat->iconAddr = load_u32be(dir_addr + PORT_CARD_DIR_OFF_ICON_ADDR);
        stat->iconFormat = load_u16be(dir_addr + PORT_CARD_DIR_OFF_ICON_FORMAT);
        stat->iconSpeed = load_u16be(dir_addr + PORT_CARD_DIR_OFF_ICON_SPEED);
        stat->commentAddr = load_u32be(dir_addr + PORT_CARD_DIR_OFF_COMMENT_ADDR);

        update_icon_offsets(dir_addr, stat);
    }

    return __CARDPutControlBlock(card, result);
}

static inline void wr32be(volatile u8* p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static inline u32 pack4_bytes(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static void dump_case(volatile u8* out, u32* w, u32 tag, s32 rc, const CARDStat* stat)
{
    wr32be(out + (*w * 4u), tag); (*w)++;
    wr32be(out + (*w * 4u), (u32)rc); (*w)++;
    wr32be(out + (*w * 4u), (u32)gc_card_block[0].result); (*w)++;
    wr32be(out + (*w * 4u), stat->length); (*w)++;
    wr32be(out + (*w * 4u), stat->time); (*w)++;
    wr32be(out + (*w * 4u), pack4_bytes((const u8*)stat->fileName + 0u)); (*w)++;
    wr32be(out + (*w * 4u), pack4_bytes((const u8*)stat->fileName + 4u)); (*w)++;
    wr32be(out + (*w * 4u), pack4_bytes((const u8*)stat->gameName + 0u)); (*w)++;
    wr32be(out + (*w * 4u),
        ((u32)(u8)stat->company[0] << 24u) |
        ((u32)(u8)stat->company[1] << 16u));
    (*w)++;
    wr32be(out + (*w * 4u), (u32)stat->bannerFormat); (*w)++;
    wr32be(out + (*w * 4u), stat->iconAddr); (*w)++;
    wr32be(out + (*w * 4u), (u32)stat->iconFormat); (*w)++;
    wr32be(out + (*w * 4u), (u32)stat->iconSpeed); (*w)++;
    wr32be(out + (*w * 4u), stat->commentAddr); (*w)++;
    wr32be(out + (*w * 4u), stat->offsetBanner); (*w)++;
    wr32be(out + (*w * 4u), stat->offsetBannerTlut); (*w)++;
    for (u32 i = 0; i < CARD_ICON_MAX; i++) {
        wr32be(out + (*w * 4u), stat->offsetIcon[i]);
        (*w)++;
    }
    wr32be(out + (*w * 4u), stat->offsetIconTlut); (*w)++;
    wr32be(out + (*w * 4u), stat->offsetData); (*w)++;
}

static void init_entry(u8* ent, const char* file_name, const char* game_name, const char* company,
                       u16 length, u32 time, u32 icon_addr, u16 icon_format,
                       u8 banner_format, u16 icon_speed, u32 comment_addr)
{
    for (u32 i = 0; i < PORT_CARD_DIR_SIZE; i++) {
        ent[i] = 0xFFu;
    }

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
    ent[PORT_CARD_DIR_OFF_PERMISSION] = PORT_CARD_ATTR_PUBLIC;
}

void oracle_card_get_status_pbt_001_suite(volatile u8* out)
{
    static u8 dir[PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE];

    memset(gc_card_block, 0, sizeof(gc_card_block));
    memset(dir, 0xFFu, sizeof(dir));

    init_entry(
      dir + 0u * PORT_CARD_DIR_SIZE,
      "slot00",
      "GAME",
      "C0",
      4u,
      0xA0A1A2A3u,
      0x00000200u,
      (CARD_STAT_ICON_C8 << 0) | (CARD_STAT_ICON_RGB5A3 << 2),
      CARD_STAT_BANNER_C8,
      (CARD_STAT_SPEED_FAST << 0) | (CARD_STAT_SPEED_MIDDLE << 2),
      0x00001000u
    );
    init_entry(
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
    init_entry(
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

    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].cblock = 64;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)dir;

    CARDStat stat;
    u32 w = 2u;

    wr32be(out + 0x00u, 0x43475330u);
    wr32be(out + 0x04u, 0x00000000u);

    memset(&stat, 0xE5u, sizeof(stat));
    s32 rc = oracle_CARDGetStatus(0, 0, &stat);
    dump_case(out, &w, 0x43475331u, rc, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    rc = oracle_CARDGetStatus(0, PORT_CARD_MAX_FILE, &stat);
    dump_case(out, &w, 0x43475332u, rc, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    rc = oracle_CARDGetStatus(-1, 0, &stat);
    dump_case(out, &w, 0x43475333u, rc, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    gc_card_block[0].attached = 0;
    rc = oracle_CARDGetStatus(0, 0, &stat);
    gc_card_block[0].attached = 1;
    dump_case(out, &w, 0x43475334u, rc, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    gc_card_block[0].result = CARD_RESULT_BUSY;
    rc = oracle_CARDGetStatus(0, 0, &stat);
    gc_card_block[0].result = CARD_RESULT_READY;
    dump_case(out, &w, 0x43475335u, rc, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    rc = oracle_CARDGetStatus(0, 1, &stat);
    dump_case(out, &w, 0x43475336u, rc, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    rc = oracle_CARDGetStatus(0, 2, &stat);
    dump_case(out, &w, 0x43475337u, rc, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    rc = oracle_CARDGetStatus(0, 3, &stat);
    dump_case(out, &w, 0x43475338u, rc, &stat);
}
