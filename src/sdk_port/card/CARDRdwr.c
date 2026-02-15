#include <stdint.h>
#include <string.h>

#include "card_bios.h"
#include "card_dir.h"
#include "card_fat.h"
#include "card_mem.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

typedef void (*CARDCallback)(s32 chan, s32 result);

enum { CARD_PAGE_SIZE = 128u };
enum { CARD_SEG_SIZE = 512u };
enum { CARD_NUM_SYSTEM_BLOCK = 5u };
enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_LIMIT = -11,
    CARD_RESULT_FATAL_ERROR = -128,
    CARD_RESULT_CANCELED = -14,
};

enum { TICKS_PER_SEC = (162000000u / 4u) };

static inline u16 rd16be(const u8* p)
{
    return (u16)(((u16)p[0] << 8u) | (u16)p[1]);
}

static inline void wr32be(u8* p, u32 v)
{
    p[0] = (u8)(v >> 24u);
    p[1] = (u8)(v >> 16u);
    p[2] = (u8)(v >> 8u);
    p[3] = (u8)v;
}

static inline int card_is_valid_block_no(const GcCardControl* card, u16 block)
{
    return ((u16)CARD_NUM_SYSTEM_BLOCK <= block && (u32)block < (u32)card->cblock);
}

static inline u32 trunc_to(u32 n, u32 a)
{
    return n & ~((u32)a - 1u);
}

static inline u16 fat_get(const u8* fat_block, u16 idx)
{
    return rd16be(fat_block + (u32)idx * 2u);
}

static u32 oracle_time_seconds(void)
{
    static u32 ticks;
    ticks += TICKS_PER_SEC;
    return ticks / TICKS_PER_SEC;
}

// Internal helpers.
s32 __CARDGetControlBlock(s32 chan, GcCardControl** pcard);
s32 __CARDPutControlBlock(GcCardControl* card, s32 result);
s32 __CARDEraseSector(s32 chan, u32 addr, CARDCallback callback);
s32 __CARDWrite(s32 chan, u32 addr, s32 length, const void* src, CARDCallback callback);
s32 __CARDWritePage(s32 chan, CARDCallback callback);
s32 __CARDUpdateDir(s32 chan, CARDCallback callback);

static void WriteCallback(s32 chan, s32 result);
static void EraseCallback(s32 chan, s32 result);

static void WriteCallback(s32 chan, s32 result)
{
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback;
    CARDFileInfo* fileInfo;

    if (result < 0) {
        goto error;
    }

    fileInfo = (CARDFileInfo*)(uintptr_t)card->fileInfo;
    if (fileInfo->length < 0) {
        result = CARD_RESULT_CANCELED;
        goto error;
    }

    fileInfo->length -= card->sector_size;
    if (fileInfo->length <= 0) {
        u8* dir = (u8*)card_ptr(card->current_dir_ptr, CARD_SYSTEM_BLOCK_SIZE);
        if (!dir) {
            result = CARD_RESULT_BROKEN;
            goto error;
        }
        u8* ent = dir + (u32)fileInfo->fileNo * (u32)PORT_CARD_DIR_SIZE;
        wr32be(ent + PORT_CARD_DIR_OFF_TIME, oracle_time_seconds());

        callback = (CARDCallback)(uintptr_t)card->api_callback;
        card->api_callback = 0u;
        result = __CARDUpdateDir(chan, callback);
    } else {
        const u8* fat = (const u8*)card_ptr(card->current_fat_ptr, CARD_SYSTEM_BLOCK_SIZE);
        if (!fat) {
            result = CARD_RESULT_BROKEN;
            goto error;
        }

        fileInfo->offset += card->sector_size;
        fileInfo->iBlock = fat_get(fat, fileInfo->iBlock);
        if (!card_is_valid_block_no(card, fileInfo->iBlock)) {
            result = CARD_RESULT_BROKEN;
            goto error;
        }
        result = __CARDEraseSector(chan, card->sector_size * (u32)fileInfo->iBlock, EraseCallback);
    }

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

static void EraseCallback(s32 chan, s32 result)
{
    GcCardControl* card;
    CARDCallback callback;
    CARDFileInfo* fileInfo;

    card = &gc_card_block[chan];
    if (result < 0) {
        goto error;
    }

    fileInfo = (CARDFileInfo*)(uintptr_t)card->fileInfo;
    result = __CARDWrite(
        chan,
        card->sector_size * (u32)fileInfo->iBlock,
        (s32)card->sector_size,
        (const void*)(uintptr_t)card->buffer,
        WriteCallback
    );
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

static void BlockWriteCallback(s32 chan, s32 result)
{
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback;

    if (result < 0) {
        goto done;
    }

    card->xferred += (s32)CARD_PAGE_SIZE;
    card->addr += (u32)CARD_PAGE_SIZE;
    card->buffer += (uintptr_t)CARD_PAGE_SIZE;
    if (--card->repeat <= 0) {
        goto done;
    }

    result = __CARDWritePage(chan, BlockWriteCallback);
    if (result >= 0) {
        return;
    }

done:
    if (card->api_callback == 0) {
        __CARDPutControlBlock(card, result);
    }
    callback = (CARDCallback)(uintptr_t)card->xfer_callback;
    if (callback) {
        card->xfer_callback = 0;
        callback(chan, result);
    }
}

s32 __CARDWrite(s32 chan, u32 addr, s32 length, const void* src, CARDCallback callback)
{
    if (chan < 0 || chan >= 2) {
        return CARD_RESULT_FATAL_ERROR;
    }
    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) {
        return CARD_RESULT_NOCARD;
    }

    card->xfer_callback = (uintptr_t)callback;
    card->repeat = (s32)(length / (s32)CARD_PAGE_SIZE);
    card->addr = addr;
    card->buffer = (uintptr_t)src;
    return __CARDWritePage(chan, BlockWriteCallback);
}

s32 __CARDSeek(CARDFileInfo* fileInfo, s32 length, s32 offset, GcCardControl** pcard)
{
    if (!fileInfo) {
        return CARD_RESULT_FATAL_ERROR;
    }

    GcCardControl* card;
    s32 result = __CARDGetControlBlock(fileInfo->chan, &card);
    if (result < 0) {
        return result;
    }

    if (!card_is_valid_block_no(card, fileInfo->iBlock) ||
        (u32)card->cblock * (u32)card->sector_size <= (u32)fileInfo->offset) {
        return __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
    }

    u8* dir = (u8*)card_ptr(card->current_dir_ptr, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }

    const u8* ent = dir + (u32)fileInfo->fileNo * (u32)PORT_CARD_DIR_SIZE;
    u16 ent_length = rd16be(ent + PORT_CARD_DIR_OFF_LENGTH);
    u16 ent_start = rd16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK);
    if ((u32)ent_length * (u32)card->sector_size <= (u32)offset ||
        (u32)ent_length * (u32)card->sector_size < (u32)offset + (u32)length) {
        return __CARDPutControlBlock(card, CARD_RESULT_LIMIT);
    }

    card->fileInfo = (uintptr_t)fileInfo;
    fileInfo->length = length;

    if (offset < fileInfo->offset) {
        fileInfo->offset = 0;
        fileInfo->iBlock = ent_start;
        if (!card_is_valid_block_no(card, fileInfo->iBlock)) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
    }

    const u8* fat = (const u8*)card_ptr(card->current_fat_ptr, CARD_SYSTEM_BLOCK_SIZE);
    if (!fat) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }

    while (trunc_to((u32)fileInfo->offset, (u32)card->sector_size) <
           trunc_to((u32)offset, (u32)card->sector_size)) {
        fileInfo->offset += card->sector_size;
        fileInfo->iBlock = fat_get(fat, fileInfo->iBlock);
        if (!card_is_valid_block_no(card, fileInfo->iBlock)) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
    }

    fileInfo->offset = offset;
    if (pcard) {
        *pcard = card;
    }
    return CARD_RESULT_READY;
}

s32 CARDWriteAsync(CARDFileInfo* fileInfo, const void* buf, s32 length, s32 offset, CARDCallback callback)
{
    GcCardControl* card;
    s32 result;

    result = __CARDSeek(fileInfo, length, offset, &card);
    if (result < 0) {
        return result;
    }

    if ((offset % card->sector_size) != 0 || (length % card->sector_size) != 0) {
        return __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
    }

    u8* dir = (u8*)card_ptr(card->current_dir_ptr, CARD_SYSTEM_BLOCK_SIZE);
    if (!dir) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }
    u8* ent = dir + (u32)fileInfo->fileNo * (u32)PORT_CARD_DIR_SIZE;
    result = __CARDAccess(card, ent);
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    card->buffer = (uintptr_t)buf;

    result = __CARDEraseSector(fileInfo->chan, card->sector_size * (u32)fileInfo->iBlock, EraseCallback);
    if (result < 0) {
        __CARDPutControlBlock(card, result);
    }
    return result;
}

s32 CARDWrite(CARDFileInfo* fileInfo, const void* buf, s32 length, s32 offset)
{
    s32 result = CARDWriteAsync(fileInfo, buf, length, offset, __CARDSyncCallback);
    if (result < 0) {
        return result;
    }
    return __CARDSync(fileInfo->chan);
}

