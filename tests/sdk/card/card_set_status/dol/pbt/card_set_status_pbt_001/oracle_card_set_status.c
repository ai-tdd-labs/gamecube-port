#include <stdint.h>
#include <stdbool.h>
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
    CARD_RESULT_NOPERM = -10,
    CARD_RESULT_FATAL_ERROR = -128,
    CARD_RESULT_BROKEN = -6,
    CARD_SYSTEM_BLOCK_SIZE = 8 * 1024,
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
        return CARD_RESULT_NOPERM;
    }
    if (card->result == CARD_RESULT_BUSY) {
        return CARD_RESULT_BUSY;
    }

    card->result = CARD_RESULT_BUSY;
    card->api_callback = 0u;
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
    u8 *p = (u8*)(uintptr_t)addr;
    return p[0];
}

static inline void store_u8(uint32_t addr, u8 v)
{
    u8 *p = (u8*)(uintptr_t)addr;
    p[0] = v;
}

static inline u16 load_u16be(uint32_t addr)
{
    u8 *p = (u8*)(uintptr_t)addr;
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static inline void store_u16be(uint32_t addr, u16 v)
{
    u8 *p = (u8*)(uintptr_t)addr;
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static inline u32 load_u32be(uint32_t addr)
{
    u8 *p = (u8*)(uintptr_t)addr;
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static inline void store_u32be(uint32_t addr, u32 v)
{
    u8 *p = (u8*)(uintptr_t)addr;
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static inline s32 card_get_banner_format(uint32_t ent_addr)
{
    return (s32)(load_u8(ent_addr + PORT_CARD_DIR_OFF_BANNER_FORMAT) & CARD_STAT_BANNER_MASK);
}

static inline s32 card_get_icon_format(uint32_t ent_addr, s32 icon_no)
{
    if (icon_no < 0 || CARD_ICON_MAX <= (u32)icon_no) {
        return CARD_STAT_ICON_NONE;
    }
    u16 iconFormat = load_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_FORMAT);
    return (s32)((iconFormat >> (icon_no * 2)) & CARD_STAT_ICON_MASK);
}

static void update_icon_offsets(uint32_t ent_addr, CARDStat *stat)
{
    u32 offset = load_u32be(ent_addr + PORT_CARD_DIR_OFF_ICON_ADDR);

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

    uint32_t iconTlut = 0u;
    for (u32 i = 0; i < CARD_ICON_MAX; i++) {
        switch (card_get_icon_format(ent_addr, (s32)i)) {
            case CARD_STAT_ICON_C8:
                stat->offsetIcon[i] = offset;
                offset += CARD_ICON_WIDTH * CARD_ICON_HEIGHT;
                iconTlut = 1u;
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
    }
    else {
        stat->offsetIconTlut = 0xFFFFFFFFu;
    }

    stat->offsetData = offset;
}

static u32 oracle_time_seconds(void)
{
    static u32 ticks;
    ticks += (162000000u / 4u);
    return ticks / (162000000u / 4u);
}

static void card_checksum_u16be(const u8 *ptr, u32 length, u16 *checksum, u16 *checksum_inv)
{
    u32 n = length / 2u;
    u16 cs = 0;
    u16 csi = 0;

    for (u32 i = 0; i < n; i++) {
        u16 v = (u16)(((u16)ptr[i * 2u] << 8) | (u16)ptr[i * 2u + 1u]);
        cs = (u16)(cs + v);
        csi = (u16)(csi + (u16)~v);
    }
    if (cs == 0xFFFFu) {
        cs = 0;
    }
    if (csi == 0xFFFFu) {
        csi = 0;
    }
    *checksum = cs;
    *checksum_inv = csi;
}

static s32 __CARDUpdateDir(s32 chan, void (*callback)(s32 chan, s32 result))
{
    GcCardControl *card = &gc_card_block[chan];
    u8 *dir;
    u32 dir_check;
    s32 checkCode;
    u16 checkSum;
    u16 checkSumInv;

    if (!card->attached) {
        return CARD_RESULT_NOPERM;
    }

    dir = (u8 *)(uintptr_t)card->current_dir_ptr;
    if (!dir) {
        return CARD_RESULT_BROKEN;
    }

    dir_check = (u32)card->current_dir_ptr + (u32)PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE;
    checkCode = (s32)load_u16be(dir_check + PORT_CARD_DIR_SIZE - 6u);
    checkCode = (checkCode + 1) & 0xFFFF;
    store_u16be(dir_check + PORT_CARD_DIR_SIZE - 6u, (u16)checkCode);

    card_checksum_u16be(dir, CARD_SYSTEM_BLOCK_SIZE - 4u, &checkSum, &checkSumInv);
    store_u16be(dir_check + PORT_CARD_DIR_SIZE - 4u, checkSum);
    store_u16be(dir_check + PORT_CARD_DIR_SIZE - 2u, checkSumInv);

    if (card->attached) {
        (void)__CARDPutControlBlock(card, CARD_RESULT_READY);
    }
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    return CARD_RESULT_READY;
}

s32 port_CARDAccess(port_CARDDirControl *card, u32 dir_entry_addr)
{
    (void)card;
    if (load_u8(dir_entry_addr + PORT_CARD_DIR_OFF_GAMENAME) == 0xFFu) {
        return CARD_RESULT_NOFILE;
    }
    return CARD_RESULT_READY;
}

s32 port_CARDIsPublic(u32 dir_entry_addr)
{
    if (load_u8(dir_entry_addr + PORT_CARD_DIR_OFF_GAMENAME) == 0xFFu) {
        return CARD_RESULT_NOFILE;
    }
    if ((load_u8(dir_entry_addr + PORT_CARD_DIR_OFF_PERMISSION) & PORT_CARD_ATTR_PUBLIC) != 0) {
        return CARD_RESULT_READY;
    }
    return CARD_RESULT_NOPERM;
}

s32 oracle_CARDSetStatus(s32 chan, s32 fileNo, CARDStat *stat, void (*callback)(s32 chan, s32 result))
{
    GcCardControl *card;
    s32 result;
    s32 view_result;
    u32 ent_addr;
    port_CARDDirControl view;

    if (fileNo < 0 || PORT_CARD_MAX_FILE <= fileNo) {
        return CARD_RESULT_FATAL_ERROR;
    }
    if (stat->iconAddr != 0xFFFFFFFFu && CARD_READ_SIZE <= stat->iconAddr) {
        return CARD_RESULT_FATAL_ERROR;
    }
    if (stat->commentAddr != 0xFFFFFFFFu &&
        (stat->commentAddr % CARD_SYSTEM_BLOCK_SIZE) >
            (CARD_SYSTEM_BLOCK_SIZE - CARD_COMMENT_SIZE)) {
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

    ent_addr = (u32)view.dir_addr + (u32)fileNo * PORT_CARD_DIR_SIZE;
    view_result = port_CARDAccess(&view, ent_addr);
    if (view_result < 0) {
        return __CARDPutControlBlock(card, view_result);
    }

    store_u8(ent_addr + PORT_CARD_DIR_OFF_BANNER_FORMAT, (u8)stat->bannerFormat);
    store_u32be(ent_addr + PORT_CARD_DIR_OFF_ICON_ADDR, stat->iconAddr);
    store_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_FORMAT, stat->iconFormat);
    store_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_SPEED, stat->iconSpeed);
    store_u32be(ent_addr + PORT_CARD_DIR_OFF_COMMENT_ADDR, stat->commentAddr);

    update_icon_offsets(ent_addr, stat);
    if (load_u32be(ent_addr + PORT_CARD_DIR_OFF_ICON_ADDR) == 0xFFFFFFFFu) {
        store_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_SPEED,
                    (u16)((load_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_SPEED) & ~(CARD_STAT_SPEED_MASK)) |
                          CARD_STAT_SPEED_FAST));
        stat->iconSpeed = CARD_STAT_SPEED_FAST;
    }

    store_u32be(ent_addr + PORT_CARD_DIR_OFF_TIME, oracle_time_seconds());
    result = __CARDUpdateDir(chan, callback);
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }
    return result;
}

s32 oracle_CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat *stat, void (*callback)(s32 chan, s32 result))
{
    return oracle_CARDSetStatus(chan, fileNo, stat, callback);
}

static s32 g_async_result = 0x11111111;

static void set_status_callback(s32 chan, s32 rc)
{
    (void)chan;
    g_async_result = rc;
}

static inline void wr32be(u8* p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static inline u32 load_u32be_const(const u8* p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static void dump_dir_entry(u8* out, u32* w, u32 ent_addr)
{
    wr32be(out + (*w * 4u), load_u32be(ent_addr + PORT_CARD_DIR_OFF_BANNER_FORMAT));
    (*w)++;
    wr32be(out + (*w * 4u), load_u32be(ent_addr + PORT_CARD_DIR_OFF_ICON_ADDR));
    (*w)++;
    wr32be(out + (*w * 4u), (u32)load_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_FORMAT));
    (*w)++;
    wr32be(out + (*w * 4u), (u32)load_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_SPEED));
    (*w)++;
    wr32be(out + (*w * 4u), load_u32be(ent_addr + PORT_CARD_DIR_OFF_COMMENT_ADDR));
    (*w)++;
    wr32be(out + (*w * 4u), load_u32be(ent_addr + PORT_CARD_DIR_OFF_TIME));
    (*w)++;
}

static void dump_stat(u8* out, u32* w, const CARDStat* stat)
{
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

static void init_entry(u8* ent, const char* file_name, const char* game_name, const char* company,
                      u16 length, u32 icon_addr, u16 icon_format,
                      u8 banner_format, u16 icon_speed, u32 comment_addr)
{
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

static void dump_case(u8* out, u32* w, u32 tag, s32 rc, u32 dir_addr, s32 file_no) {
    u32 entry = dir_addr + (u32)file_no * PORT_CARD_DIR_SIZE;

    wr32be(out + (*w * 4u), tag);
    (*w)++;
    wr32be(out + (*w * 4u), (u32)rc);
    (*w)++;
    wr32be(out + (*w * 4u), (u32)gc_card_block[0].result);
    (*w)++;

    dump_dir_entry(out, w, entry);
}

void oracle_card_set_status_pbt_001_suite(u8* out)
{
    static u8 dir[PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE + CARD_SYSTEM_BLOCK_SIZE];
    memset(gc_card_block, 0, sizeof(gc_card_block));
    memset(dir, 0xFFu, sizeof(dir));

    init_entry(dir + 0u * PORT_CARD_DIR_SIZE,
               "slot00",
               "GAME",
               "C0",
               4u,
               0x00000200u,
               (u16)((CARD_STAT_ICON_C8 << 0) | (CARD_STAT_ICON_RGB5A3 << 2)),
               CARD_STAT_BANNER_C8,
               (u16)((CARD_STAT_SPEED_FAST << 0) | (CARD_STAT_SPEED_MIDDLE << 2)),
               0x00001000u);
    init_entry(dir + 1u * PORT_CARD_DIR_SIZE,
               "slot01",
               "GAME",
               "C0",
               1u,
               0xFFFFFFFFu,
               CARD_STAT_ICON_NONE,
               CARD_STAT_BANNER_NONE,
               0x00000000u,
               0x00002000u);

    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].cblock = 64;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)dir;

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
    s32 rc = oracle_CARDSetStatus(0, 0, &stat, NULL);
    dump_case(out, &w, 0x434E534Bu, rc, (u32)(uintptr_t)dir, 0);
    dump_stat(out, &w, &stat);

    rc = oracle_CARDSetStatus(0, PORT_CARD_MAX_FILE, &stat, NULL);
    memset(&stat, 0xE5u, sizeof(stat));
    dump_case(out, &w, 0x434E5342u, rc, (u32)(uintptr_t)dir, 0);
    dump_stat(out, &w, &stat);

    rc = oracle_CARDSetStatus(-1, 0, &stat, NULL);
    memset(&stat, 0xE5u, sizeof(stat));
    dump_case(out, &w, 0x434E5343u, rc, (u32)(uintptr_t)dir, 0);
    dump_stat(out, &w, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = CARD_READ_SIZE;
    stat.iconFormat = (u16)(CARD_STAT_ICON_C8 << 0);
    stat.iconSpeed = (u16)CARD_STAT_SPEED_FAST;
    stat.commentAddr = 0x00004000u;
    rc = oracle_CARDSetStatus(0, 0, &stat, NULL);
    dump_case(out, &w, 0x434E5344u, rc, (u32)(uintptr_t)dir, 0);
    dump_stat(out, &w, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = 0x00000400u;
    stat.iconFormat = (u16)(CARD_STAT_ICON_RGB5A3 << 0);
    stat.iconSpeed = (u16)CARD_STAT_SPEED_MIDDLE;
    stat.commentAddr = 0x1FE9u;
    rc = oracle_CARDSetStatus(0, 0, &stat, NULL);
    dump_case(out, &w, 0x434E5345u, rc, (u32)(uintptr_t)dir, 0);
    dump_stat(out, &w, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = 0x00000400u;
    stat.iconFormat = (u16)(CARD_STAT_ICON_C8 << 0);
    stat.iconSpeed = (u16)CARD_STAT_SPEED_MIDDLE;
    stat.commentAddr = 0x00003000u;
    gc_card_block[0].attached = 0;
    rc = oracle_CARDSetStatus(0, 0, &stat, NULL);
    gc_card_block[0].attached = 1;
    dump_case(out, &w, 0x434E5346u, rc, (u32)(uintptr_t)dir, 0);
    dump_stat(out, &w, &stat);

    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = 0xDEADBEEFu;
    stat.iconAddr = 0xFFFFFFFFu;
    stat.iconFormat = (u16)0xBADCu;
    stat.iconSpeed = 0x1111u;
    stat.commentAddr = 0x00005000u;
    rc = oracle_CARDSetStatus(0, 1, &stat, NULL);
    dump_case(out, &w, 0x434E5347u, rc, (u32)(uintptr_t)dir, 1);
    dump_stat(out, &w, &stat);

    g_async_result = 0x11111111u;
    memset(&stat, 0xE5u, sizeof(stat));
    stat.bannerFormat = CARD_STAT_BANNER_RGB5A3;
    stat.iconAddr = 0x00000400u;
    stat.iconFormat = (u16)(CARD_STAT_ICON_C8 << 0);
    stat.iconSpeed = (u16)CARD_STAT_SPEED_MIDDLE;
    stat.commentAddr = 0x00003000u;
    rc = oracle_CARDSetStatusAsync(0, 0, &stat, set_status_callback);
    wr32be(out + (u32)(w * 4u), 0x434E534Au);
    w++;
    wr32be(out + (u32)(w * 4u), (u32)rc);
    w++;
    wr32be(out + (u32)(w * 4u), (u32)g_async_result);
    w++;
    wr32be(out + (u32)(w * 4u), (u32)gc_card_block[0].result);
    w++;
    dump_stat(out, &w, &stat);

    memset(dir + 1u * PORT_CARD_DIR_SIZE, 0xFFu, PORT_CARD_DIR_SIZE);
    rc = oracle_CARDSetStatus(0, 1, &stat, NULL);
    dump_case(out, &w, 0x434E5348u, rc, (u32)(uintptr_t)dir, 1);
    dump_stat(out, &w, &stat);
}
