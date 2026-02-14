#include <stdint.h>
#include "../gc_mem.h"

#include "card_bios.h"
#include "card_dir.h"
#include "card_fat.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BROKEN = -6,
};

extern s32 __CARDGetControlBlock(s32 chan, GcCardControl **pcard);
extern s32 __CARDPutControlBlock(GcCardControl *card, s32 result);

static inline u16 load_u16be(uint32_t addr)
{
    uint8_t *p = gc_mem_ptr(addr, sizeof(u16));
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed)
{
    GcCardControl* card;
    s32 result;
    u32 i;

    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    if (!card->current_fat_ptr || !card->current_dir_ptr) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }

    if (byteNotUsed) {
        u16 freeBlocks = load_u16be((u32)card->current_fat_ptr + (u32)PORT_CARD_FAT_FREEBLOCKS * 2u);
        *byteNotUsed = (s32)(freeBlocks * card->sector_size);
    }

    if (filesNotUsed) {
        *filesNotUsed = 0;
        for (i = 0; i < PORT_CARD_MAX_FILE; ++i) {
            u32 fileNameOff = (u32)card->current_dir_ptr + i * PORT_CARD_DIR_SIZE + PORT_CARD_DIR_OFF_FILENAME;
            const uint8_t* filename = gc_mem_ptr(fileNameOff, 1u);
            if (filename[0] == 0xFFu) {
                ++(*filesNotUsed);
            }
        }
    }

    return __CARDPutControlBlock(card, CARD_RESULT_READY);
}
