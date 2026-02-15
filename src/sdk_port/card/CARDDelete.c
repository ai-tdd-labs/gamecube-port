#include <stdint.h>
#include <string.h>

#include "card_bios.h"
#include "card_dir.h"
#include "card_mem.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

typedef void (*CARDCallback)(s32 chan, s32 result);

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BUSY = -1,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_FATAL_ERROR = -128,
};

static inline u16 rd16be(const u8* p)
{
    return (u16)(((u16)p[0] << 8u) | (u16)p[1]);
}

// Internal helpers.
s32 __CARDGetControlBlock(s32 chan, GcCardControl** pcard);
s32 __CARDPutControlBlock(GcCardControl* card, s32 result);
s32 __CARDFreeBlock(s32 chan, u16 nBlock, CARDCallback callback);
s32 __CARDUpdateDir(s32 chan, CARDCallback callback);

static void DeleteCallback(s32 chan, s32 result)
{
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;

    if (result < 0) {
        goto error;
    }

    result = __CARDFreeBlock(chan, card->startBlock, callback);
    if (result < 0) {
        goto error;
    }
    return;

error:
    __CARDPutControlBlock(card, result);
    if (callback) {
        callback(chan, result);
    }
}

s32 CARDFastDeleteAsync(s32 chan, s32 fileNo, CARDCallback callback)
{
    if (fileNo < 0 || PORT_CARD_MAX_FILE <= fileNo) {
        return CARD_RESULT_FATAL_ERROR;
    }

    GcCardControl* card;
    s32 result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    u8* dir = (u8*)card_ptr(card->current_dir_ptr, 8u * 1024u);
    if (!dir) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }

    u8* ent = dir + (u32)fileNo * (u32)PORT_CARD_DIR_SIZE;
    result = __CARDAccess(card, ent);
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }
    if (__CARDIsOpened(card, fileNo)) {
        return __CARDPutControlBlock(card, CARD_RESULT_BUSY);
    }

    card->startBlock = rd16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK);
    memset(ent, 0xFFu, PORT_CARD_DIR_SIZE);

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    result = __CARDUpdateDir(chan, DeleteCallback);
    if (result < 0) {
        __CARDPutControlBlock(card, result);
    }
    return result;
}

s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback)
{
    GcCardControl* card;
    s32 fileNo;
    s32 result;

    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    result = __CARDGetFileNo(card, fileName, &fileNo);
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }
    if (__CARDIsOpened(card, fileNo)) {
        return __CARDPutControlBlock(card, CARD_RESULT_BUSY);
    }

    u8* dir = (u8*)card_ptr(card->current_dir_ptr, 8u * 1024u);
    if (!dir) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }
    u8* ent = dir + (u32)fileNo * (u32)PORT_CARD_DIR_SIZE;
    card->startBlock = rd16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK);
    memset(ent, 0xFFu, PORT_CARD_DIR_SIZE);

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    result = __CARDUpdateDir(chan, DeleteCallback);
    if (result < 0) {
        __CARDPutControlBlock(card, result);
    }
    return result;
}

s32 CARDDelete(s32 chan, const char* fileName)
{
    s32 result = CARDDeleteAsync(chan, fileName, __CARDSyncCallback);
    if (result < 0) {
        return result;
    }
    return __CARDSync(chan);
}
