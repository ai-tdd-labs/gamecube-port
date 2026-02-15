#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "card_bios.h"
#include "card_dir.h"
#include "card_mem.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef int64_t s64;
typedef s64 OSTime;
typedef void (*CARDCallback)(s32 chan, s32 result);

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_NOPERM = -10,
    CARD_RESULT_NOFILE = -4,
    CARD_RESULT_FATAL_ERROR = -128,
    CARD_SYSTEM_BLOCK_SIZE = 8 * 1024,
};

static inline u8 rd_u8(uintptr_t addr)
{
    u8 *p = (u8*)card_ptr(addr, 1);
    return p ? *p : 0;
}

static inline void wr_u8(uintptr_t addr, u8 v)
{
    u8 *p = (u8*)card_ptr(addr, 1);
    if (p) {
        *p = v;
    }
}

static inline u16 rd_u16be(uintptr_t addr)
{
    u8 *p = (u8*)card_ptr(addr, 2);
    if (!p) {
        return 0;
    }
    return (u16)(((u16)p[0] << 8) | p[1]);
}

static inline void wr_u16be(uintptr_t addr, u16 v)
{
    u8 *p = (u8*)card_ptr(addr, 2);
    if (!p) {
        return;
    }
    p[0] = (u8)(v >> 8);
    p[1] = (u8)(v);
}

static inline u32 rd_u32be(uintptr_t addr)
{
    u8 *p = (u8*)card_ptr(addr, 4);
    if (!p) {
        return 0;
    }
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static inline void wr_u32be(uintptr_t addr, u32 v)
{
    u8 *p = (u8*)card_ptr(addr, 4);
    if (!p) {
        return;
    }
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)(v);
}

static inline s32 card_get_banner_format(u32 ent_addr)
{
    return (s32)(rd_u8(ent_addr + PORT_CARD_DIR_OFF_BANNER_FORMAT) & CARD_STAT_BANNER_MASK);
}

static inline s32 card_get_icon_format(u32 ent_addr, s32 icon_no)
{
    if (icon_no < 0 || CARD_ICON_MAX <= icon_no) {
        return CARD_STAT_ICON_NONE;
    }
    u16 iconFormat = rd_u16be((uintptr_t)ent_addr + PORT_CARD_DIR_OFF_ICON_FORMAT);
    return (s32)((iconFormat >> (icon_no * 2)) & CARD_STAT_ICON_MASK);
}

static void update_icon_offsets(u32 ent_addr, CARDStat* stat)
{
    u32 offset = rd_u32be((uintptr_t)ent_addr + PORT_CARD_DIR_OFF_ICON_ADDR);

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

    bool iconTlut = false;
    for (u32 i = 0; i < CARD_ICON_MAX; i++) {
        switch (card_get_icon_format(ent_addr, (s32)i)) {
            case CARD_STAT_ICON_C8:
                stat->offsetIcon[i] = offset;
                offset += CARD_ICON_WIDTH * CARD_ICON_HEIGHT;
                iconTlut = true;
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

static inline u32 ticks_to_seconds(OSTime ticks)
{
    return (u32)(ticks / (162000000u / 4u));
}

static OSTime OSGetTime(void)
{
    static OSTime s_ticks;
    s_ticks += (OSTime)(162000000u / 4u);
    return s_ticks;
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

int32_t __CARDUpdateDir(s32 chan, CARDCallback callback)
{
    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return CARD_RESULT_FATAL_ERROR;
    }

    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) {
        return CARD_RESULT_NOCARD;
    }

    uintptr_t dir = card->current_dir_ptr;
    if (!dir) {
        return CARD_RESULT_BROKEN;
    }

    u8 *dir_block = (u8*)card_ptr(dir, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir_block) {
        return CARD_RESULT_BROKEN;
    }

    uintptr_t dir_check = dir + (uintptr_t)PORT_CARD_MAX_FILE * (uintptr_t)PORT_CARD_DIR_SIZE;
    u16 checkCode = rd_u16be(dir_check + PORT_CARD_DIR_SIZE - 6u);
    checkCode = (u16)(checkCode + 1u);
    wr_u16be(dir_check + PORT_CARD_DIR_SIZE - 6u, checkCode);

    u16 checkSum;
    u16 checkSumInv;
    card_checksum_u16be(dir_block, CARD_SYSTEM_BLOCK_SIZE - 4u, &checkSum, &checkSumInv);
    wr_u16be(dir_check + PORT_CARD_DIR_SIZE - 4u, checkSum);
    wr_u16be(dir_check + PORT_CARD_DIR_SIZE - 2u, checkSumInv);

    if (card->attached) {
        (void)__CARDPutControlBlock(card, CARD_RESULT_READY);
    }

    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    return CARD_RESULT_READY;
}

s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat *stat, CARDCallback callback)
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
        (stat->commentAddr % CARD_SYSTEM_BLOCK_SIZE) > (CARD_SYSTEM_BLOCK_SIZE - CARD_COMMENT_SIZE)) {
        return CARD_RESULT_FATAL_ERROR;
    }

    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    memset(&view, 0, sizeof(view));
    view.attached = card->attached;
    view.cBlock = (uint16_t)card->cblock;
    view.sectorSize = card->sector_size;
    view.dir_addr = (u32)card->current_dir_ptr;
    view.fat_addr = (u32)card->current_fat_ptr;
    view.diskID_is_none = 1;

    ent_addr = (u32)view.dir_addr + (u32)fileNo * PORT_CARD_DIR_SIZE;
    view_result = port_CARDAccess(&view, ent_addr);
    if (view_result == CARD_RESULT_NOPERM) {
        view_result = port_CARDIsPublic(ent_addr);
    }
    if (view_result < 0) {
        return __CARDPutControlBlock(card, view_result);
    }

    wr_u8(ent_addr + PORT_CARD_DIR_OFF_BANNER_FORMAT, stat->bannerFormat);
    wr_u32be(ent_addr + PORT_CARD_DIR_OFF_ICON_ADDR, stat->iconAddr);
    wr_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_FORMAT, stat->iconFormat);
    wr_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_SPEED, stat->iconSpeed);
    wr_u32be(ent_addr + PORT_CARD_DIR_OFF_COMMENT_ADDR, stat->commentAddr);

    update_icon_offsets(ent_addr, stat);
    if (rd_u32be(ent_addr + PORT_CARD_DIR_OFF_ICON_ADDR) == 0xFFFFFFFFu) {
        wr_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_SPEED,
            (u16)((rd_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_SPEED) & ~(CARD_STAT_SPEED_MASK)) |
            CARD_STAT_SPEED_FAST));
        stat->iconSpeed = CARD_STAT_SPEED_FAST;
    }

    wr_u32be(ent_addr + PORT_CARD_DIR_OFF_TIME, ticks_to_seconds(OSGetTime()));
    result = __CARDUpdateDir(chan, callback);
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }
    return result;
}

s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat *stat)
{
    s32 result = CARDSetStatusAsync(chan, fileNo, stat, __CARDSyncCallback);
    if (result < 0) {
        return result;
    }
    return __CARDSync(chan);
}
