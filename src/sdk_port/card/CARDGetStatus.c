#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "card_bios.h"
#include "card_dir.h"
#include "card_mem.h"

extern int32_t __CARDGetControlBlock(int32_t chan, GcCardControl **pcard);
extern int32_t __CARDPutControlBlock(GcCardControl *card, int32_t result);

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_FATAL_ERROR = -128,
};

static inline u8 load_u8(uintptr_t addr)
{
    u8 *p = (u8*)card_ptr(addr, 1);
    return p ? p[0] : 0;
}

static inline u16 load_u16be(uintptr_t addr)
{
    u8 *p = (u8*)card_ptr(addr, 2);
    if (!p) {
        return 0;
    }
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static inline u32 load_u32be(uintptr_t addr)
{
    u8 *p = (u8*)card_ptr(addr, 4);
    if (!p) {
        return 0;
    }
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static inline s32 card_get_banner_format(uintptr_t ent_addr)
{
    return (s32)(load_u8(ent_addr + PORT_CARD_DIR_OFF_BANNER_FORMAT) & CARD_STAT_BANNER_MASK);
}

static inline s32 card_get_icon_format(uintptr_t ent_addr, s32 icon_no)
{
    if (icon_no < 0 || CARD_ICON_MAX <= icon_no) {
        return CARD_STAT_ICON_NONE;
    }
    u16 iconFormat = load_u16be(ent_addr + PORT_CARD_DIR_OFF_ICON_FORMAT);
    return (s32)((iconFormat >> (icon_no * 2)) & CARD_STAT_ICON_MASK);
}

static void update_icon_offsets(uintptr_t ent_addr, CARDStat* stat)
{
    u32 offset;
    bool iconTlut = 0;

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
    }
    else {
        stat->offsetIconTlut = 0xFFFFFFFFu;
    }

    stat->offsetData = offset;
}

s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat)
{
    GcCardControl* card;
    s32 result;
    uintptr_t dir_addr;

    if (fileNo < 0 || PORT_CARD_MAX_FILE <= fileNo) {
        return CARD_RESULT_FATAL_ERROR;
    }

    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    dir_addr = (uintptr_t)card->current_dir_ptr + (uintptr_t)fileNo * (uintptr_t)PORT_CARD_DIR_SIZE;

    // Match decomp: __CARDAccess then __CARDIsPublic fallback.
    port_CARDDirControl view;
    memset(&view, 0, sizeof(view));
    view.attached = card->attached;
    view.cBlock = (u16)card->cblock;
    view.sectorSize = card->sector_size;
    view.dir_addr = (u32)card->current_dir_ptr;
    view.fat_addr = (u32)card->current_fat_ptr;
    view.diskID_is_none = 1;

    result = port_CARDAccess(&view, dir_addr);
    if (result == PORT_CARD_DIR_RESULT_NOPERM) {
        result = port_CARDIsPublic(dir_addr);
    }

    if (result >= 0) {
        const void* gn = card_ptr(dir_addr + PORT_CARD_DIR_OFF_GAMENAME, 4);
        const void* co = card_ptr(dir_addr + PORT_CARD_DIR_OFF_COMPANY, 2);
        if (!gn || !co) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
        memcpy(stat->gameName, gn, 4);
        memcpy(stat->company, co, 2);
        stat->length = load_u16be(dir_addr + PORT_CARD_DIR_OFF_LENGTH) * (u32)card->sector_size;
        const void* fn = card_ptr(dir_addr + PORT_CARD_DIR_OFF_FILENAME, CARD_FILENAME_MAX);
        if (!fn) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
        memcpy(stat->fileName, fn, CARD_FILENAME_MAX);
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
