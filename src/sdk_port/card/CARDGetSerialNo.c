#include <stdint.h>

#include "card_bios.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BROKEN = -6,
};

extern int __CARDGetControlBlock(int32_t chan, GcCardControl** pcard);
extern int __CARDPutControlBlock(GcCardControl* card, s32 result);

s32 CARDGetSerialNo(s32 chan, u64* serialNo)
{
    GcCardControl* card;
    s32 result;
    u64 code;
    s32 i;
    s32 b;

    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    code = 0;
    for (i = 0; i < 4; i++) {
        u64 piece = 0;
        u8* serial = (u8*)(uintptr_t)card->work_area + (u32)(u32)i * 8u;

        for (b = 0; b < 8; b++) {
            piece = (piece << 8) | (u64)serial[b];
        }
        code ^= piece;
    }

    if (serialNo) {
        *serialNo = code;
    }
    return __CARDPutControlBlock(card, CARD_RESULT_READY);
}

