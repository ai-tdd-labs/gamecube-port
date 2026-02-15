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

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_EXIST = -7,
    CARD_RESULT_NOENT = -8,
    CARD_RESULT_INSSPACE = -9,
    CARD_RESULT_FATAL_ERROR = -128,
    CARD_RESULT_NAMETOOLONG = -12,
};

enum { TICKS_PER_SEC = (162000000u / 4u) };

static inline u16 rd16be(const u8* p)
{
    return (u16)(((u16)p[0] << 8u) | (u16)p[1]);
}

static inline void wr16be(u8* p, u16 v)
{
    p[0] = (u8)(v >> 8u);
    p[1] = (u8)v;
}

static inline void wr32be(u8* p, u32 v)
{
    p[0] = (u8)(v >> 24u);
    p[1] = (u8)(v >> 16u);
    p[2] = (u8)(v >> 8u);
    p[3] = (u8)v;
}

static int card_compare_filename(const u8* entry, const char* file_name)
{
    for (u32 i = 0; i < (u32)PORT_CARD_FILENAME_MAX; i++) {
        char a = (char)entry[PORT_CARD_DIR_OFF_FILENAME + i];
        char b = file_name[i];
        if (a != b) {
            return 0;
        }
        if (a == '\0') {
            return 1;
        }
    }
    return file_name[PORT_CARD_FILENAME_MAX] == '\0';
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
s32 __CARDAllocBlock(s32 chan, u32 cBlock, CARDCallback callback);
s32 __CARDUpdateDir(s32 chan, CARDCallback callback);

static void CreateCallbackFat(s32 chan, s32 result)
{
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;

    if (result < 0) {
        goto error;
    }

    u8* dir = (u8*)card_ptr(card->current_dir_ptr, 8u * 1024u);
    if (!dir) {
        result = CARD_RESULT_FATAL_ERROR;
        goto error;
    }

    u8* ent = dir + (u32)card->freeNo * (u32)PORT_CARD_DIR_SIZE;
    memcpy(ent + PORT_CARD_DIR_OFF_GAMENAME, card->id, 4);
    memcpy(ent + PORT_CARD_DIR_OFF_COMPANY, card->id + 4, 2);

    ent[PORT_CARD_DIR_OFF_PERMISSION] = (u8)PORT_CARD_ATTR_PUBLIC;
    ent[PORT_CARD_DIR_OFF_COPY_TIMES] = 0;
    wr16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK, card->startBlock);

    ent[PORT_CARD_DIR_OFF_BANNER_FORMAT] = 0;
    wr32be(ent + PORT_CARD_DIR_OFF_ICON_ADDR, 0xFFFFFFFFu);
    wr16be(ent + PORT_CARD_DIR_OFF_ICON_FORMAT, 0);
    // icon0 speed FAST => 1 in low 2 bits
    wr16be(ent + PORT_CARD_DIR_OFF_ICON_SPEED, 1u);
    wr32be(ent + PORT_CARD_DIR_OFF_COMMENT_ADDR, 0xFFFFFFFFu);

    CARDFileInfo* fi = (CARDFileInfo*)(uintptr_t)card->fileInfo;
    if (fi) {
        fi->offset = 0;
        fi->iBlock = card->startBlock;
    }

    wr32be(ent + PORT_CARD_DIR_OFF_TIME, oracle_time_seconds());

    result = __CARDUpdateDir(chan, callback);
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

s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, CARDCallback callback)
{
    if (!fileName) {
        return CARD_RESULT_FATAL_ERROR;
    }
    if (strlen(fileName) > (u32)PORT_CARD_FILENAME_MAX) {
        return CARD_RESULT_NAMETOOLONG;
    }

    GcCardControl* card;
    s32 result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    if (size == 0u || (size % (u32)card->sector_size) != 0u) {
        return __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
    }

    u8* dir = (u8*)card_ptr(card->current_dir_ptr, 8u * 1024u);
    if (!dir) {
        return __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
    }

    u16 freeNo = (u16)0xFFFFu;
    for (u16 fileNo = 0u; fileNo < (u16)PORT_CARD_MAX_FILE; fileNo++) {
        const u8* ent = dir + (u32)fileNo * (u32)PORT_CARD_DIR_SIZE;
        if (ent[PORT_CARD_DIR_OFF_GAMENAME] == 0xFFu) {
            if (freeNo == 0xFFFFu) {
                freeNo = fileNo;
            }
            continue;
        }
        if (memcmp(ent + PORT_CARD_DIR_OFF_GAMENAME, card->id, 4) == 0 &&
            memcmp(ent + PORT_CARD_DIR_OFF_COMPANY, card->id + 4, 2) == 0 &&
            card_compare_filename(ent, fileName)) {
            return __CARDPutControlBlock(card, CARD_RESULT_EXIST);
        }
    }

    if (freeNo == 0xFFFFu) {
        return __CARDPutControlBlock(card, CARD_RESULT_NOENT);
    }

    u8* fat = (u8*)card_ptr(card->current_fat_ptr, 8u * 1024u);
    if (!fat) {
        return __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
    }

    u16 freeBlocks = rd16be(fat + (u32)PORT_CARD_FAT_FREEBLOCKS * 2u);
    if ((u32)card->sector_size * (u32)freeBlocks < size) {
        return __CARDPutControlBlock(card, CARD_RESULT_INSSPACE);
    }

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    card->freeNo = freeNo;

    u8* ent = dir + (u32)freeNo * (u32)PORT_CARD_DIR_SIZE;
    wr16be(ent + PORT_CARD_DIR_OFF_LENGTH, (u16)(size / (u32)card->sector_size));
    memset(ent + PORT_CARD_DIR_OFF_FILENAME, 0, PORT_CARD_FILENAME_MAX);
    for (u32 i = 0; i < (u32)PORT_CARD_FILENAME_MAX; i++) {
        ent[PORT_CARD_DIR_OFF_FILENAME + i] = (u8)fileName[i];
        if (fileName[i] == '\0') {
            break;
        }
    }

    card->fileInfo = (uintptr_t)fileInfo;
    if (fileInfo) {
        fileInfo->chan = chan;
        fileInfo->fileNo = (s32)freeNo;
    }

    result = __CARDAllocBlock(chan, size / (u32)card->sector_size, CreateCallbackFat);
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }
    return result;
}

s32 CARDCreate(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo)
{
    s32 result = CARDCreateAsync(chan, fileName, size, fileInfo, __CARDSyncCallback);
    if (result < 0) {
        return result;
    }
    return __CARDSync(chan);
}

