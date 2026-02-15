#include <stdint.h>

#include "card_bios.h"
#include "card_dir.h"
#include "card_mem.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

typedef void (*CARDCallback)(s32 chan, s32 result);

enum { CARD_SEG_SIZE = 512u };
enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BUSY = -1,
    CARD_RESULT_NOPERM = -10,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_CANCELED = -14,
    CARD_RESULT_FATAL_ERROR = -128,
};

static inline u32 trunc_to(u32 n, u32 a) { return n & ~(a - 1u); }
static inline u32 offset_in(u32 n, u32 a) { return n & (a - 1u); }

static inline u16 rd16be(const u8* p)
{
    return (u16)(((u16)p[0] << 8u) | (u16)p[1]);
}

static inline u16 fat_get(const u8* fat, u16 idx)
{
    return rd16be(fat + (u32)idx * 2u);
}

// Internal helpers.
s32 __CARDGetControlBlock(s32 chan, GcCardControl** pcard);
s32 __CARDPutControlBlock(GcCardControl* card, s32 result);
s32 __CARDRead(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback);
s32 __CARDSeek(CARDFileInfo* fileInfo, s32 length, s32 offset, GcCardControl** pcard);

static void DCInvalidateRange(void* ptr, u32 length)
{
    (void)ptr;
    (void)length;
}

static void ReadCallback(s32 chan, s32 result)
{
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback;
    const u8* fat;
    CARDFileInfo* fileInfo;
    s32 len_to_boundary;

    if (result < 0) {
        goto error;
    }

    fileInfo = (CARDFileInfo*)(uintptr_t)card->fileInfo;
    if (fileInfo->length < 0) {
        result = CARD_RESULT_CANCELED;
        goto error;
    }

    len_to_boundary = (s32)trunc_to((u32)fileInfo->offset + (u32)card->sector_size, (u32)card->sector_size) - fileInfo->offset;
    fileInfo->length -= len_to_boundary;
    if (fileInfo->length <= 0) {
        goto error;
    }

    fat = (const u8*)card_ptr_ro(card->current_fat_ptr, CARD_SYSTEM_BLOCK_SIZE);
    if (!fat) {
        result = CARD_RESULT_BROKEN;
        goto error;
    }
    fileInfo->offset += len_to_boundary;
    fileInfo->iBlock = fat_get(fat, fileInfo->iBlock);
    if (fileInfo->iBlock < 5u || (u32)fileInfo->iBlock >= (u32)card->cblock) {
        result = CARD_RESULT_BROKEN;
        goto error;
    }

    result = __CARDRead(chan,
                        card->sector_size * (u32)fileInfo->iBlock,
                        (fileInfo->length < (s32)card->sector_size) ? fileInfo->length : (s32)card->sector_size,
                        (void*)(uintptr_t)card->buffer,
                        ReadCallback);
    if (result < 0) {
        goto error;
    }
    return;

error:
    callback = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;
    __CARDPutControlBlock(card, result);
    if (callback) {
        callback(chan, result);
    }
}

s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback)
{
    GcCardControl* card;
    s32 result;

    if (offset_in((u32)offset, CARD_SEG_SIZE) != 0u || offset_in((u32)length, CARD_SEG_SIZE) != 0u) {
        return CARD_RESULT_FATAL_ERROR;
    }

    result = __CARDSeek(fileInfo, length, offset, &card);
    if (result < 0) {
        return result;
    }

    u8* dir = (u8*)card_ptr(card->current_dir_ptr, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }
    u8* ent = dir + (u32)fileInfo->fileNo * (u32)PORT_CARD_DIR_SIZE;
    result = __CARDAccess(card, ent);
    if (result == CARD_RESULT_NOPERM) {
        result = __CARDIsPublic(ent);
    }
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }

    DCInvalidateRange(buf, (u32)length);
    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);

    s32 off_in_sector = (s32)offset_in((u32)fileInfo->offset, (u32)card->sector_size);
    s32 first_len = (length < (s32)card->sector_size - off_in_sector) ? length : (s32)card->sector_size - off_in_sector;
    result = __CARDRead(fileInfo->chan,
                        card->sector_size * (u32)fileInfo->iBlock + (u32)off_in_sector,
                        first_len,
                        buf,
                        ReadCallback);
    if (result < 0) {
        __CARDPutControlBlock(card, result);
    }
    return result;
}

s32 CARDRead(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset)
{
    s32 result = CARDReadAsync(fileInfo, buf, length, offset, __CARDSyncCallback);
    if (result < 0) {
        return result;
    }
    return __CARDSync(fileInfo->chan);
}
