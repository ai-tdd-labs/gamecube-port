#include <stdint.h>
#include "card_bios.h"
#include "card_dir.h"
#include "../gc_mem.h"

#include <string.h>

typedef int32_t s32;
typedef uint16_t u16;
typedef uint8_t u8;

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BROKEN = -6,
};

// Internal helpers from card_bios.c.
s32 __CARDGetControlBlock(int32_t chan, GcCardControl** pcard);
s32 __CARDPutControlBlock(GcCardControl* card, s32 result);

static inline u16 be16(const u8* p) {
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static inline int card_is_valid_block_no(port_CARDDirControl* card, u16 iBlock) {
    return (PORT_CARD_DIR_NUM_SYSTEM_BLOCK <= iBlock && iBlock < card->cBlock);
}

s32 CARDOpen(int32_t chan, const char* fileName, CARDFileInfo* fileInfo) {
    GcCardControl* card;
    port_CARDDirControl view;
    s32 result;
    s32 fileNo;
    const u8* dir;
    u16 startBlock;

    fileInfo->chan = -1;
    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    memset(&view, 0, sizeof(view));
    view.attached = card->attached;
    view.cBlock = (uint16_t)card->cblock;
    view.sectorSize = card->sector_size;
    view.dir_addr = (uint32_t)card->current_dir_ptr;
    view.fat_addr = (uint32_t)card->current_fat_ptr;
    view.diskID_is_none = 1;

    result = port_CARDGetFileNo(&view, fileName, &fileNo);
    if (0 <= result) {
        dir = (const u8*)gc_mem_ptr((uint32_t)card->current_dir_ptr, PORT_CARD_DIR_SIZE);
        if (!dir) {
            result = CARD_RESULT_BROKEN;
        } else {
        startBlock = be16(dir + (uint32_t)fileNo * PORT_CARD_DIR_SIZE +
                         PORT_CARD_DIR_OFF_STARTBLOCK);

        if (!card_is_valid_block_no(&view, startBlock)) {
            result = CARD_RESULT_BROKEN;
        } else {
            fileInfo->chan = chan;
            fileInfo->fileNo = fileNo;
            fileInfo->offset = 0;
            fileInfo->iBlock = startBlock;
        }
        }
    }

    return __CARDPutControlBlock(card, result);
}

s32 CARDClose(CARDFileInfo* fileInfo) {
    GcCardControl* card;
    s32 result;

    result = __CARDGetControlBlock(fileInfo->chan, &card);
    if (result < 0) {
        return result;
    }

    fileInfo->chan = -1;
    return __CARDPutControlBlock(card, CARD_RESULT_READY);
}
