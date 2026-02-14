#include <stdint.h>

#include "card_bios.h"

typedef uint32_t u32;
typedef int32_t s32;

enum {
    CARD_RESULT_READY = 0,
};

extern s32 __CARDGetControlBlock(s32 chan, GcCardControl **pcard);
extern s32 __CARDPutControlBlock(GcCardControl *card, s32 result);

s32 CARDGetSectorSize(s32 chan, u32* size)
{
    GcCardControl* card;
    s32 result;

    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    if (size) {
        *size = (u32)card->sector_size;
    }
    return __CARDPutControlBlock(card, CARD_RESULT_READY);
}

