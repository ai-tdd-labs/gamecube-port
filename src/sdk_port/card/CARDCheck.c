#include <stdint.h>
#include <string.h>

#include "card_bios.h"
#include "dolphin/OSRtcPriv.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;

s32 __CARDUpdateDir(s32 chan, CARDCallback callback);

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_ENCODING = -13,
    CARD_RESULT_FATAL_ERROR = -128,
};

enum { CARD_SYSTEM_BLOCK_SIZE = 8 * 1024 };
enum { CARD_NUM_SYSTEM_BLOCK = 5 };
enum { CARD_MAX_FILE = 127 };
enum { CARD_DIR_SIZE = 64 };

enum {
    CARD_FAT_AVAIL = 0x0000u,
    CARD_FAT_CHECKSUM = 0x0000u,
    CARD_FAT_CHECKSUMINV = 0x0001u,
    CARD_FAT_CHECKCODE = 0x0002u,
    CARD_FAT_FREEBLOCKS = 0x0003u,
};

enum {
    PORT_CARD_DIR_OFF_GAMENAME = 0u,
    PORT_CARD_DIR_OFF_STARTBLOCK = 54u,
    PORT_CARD_DIR_OFF_LENGTH = 56u,
};

extern u16 OSGetFontEncode(void);

static inline void wr16be(u8 *p, u16 v);

static inline u16 rd16be(const u8 *p)
{
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static inline u16 rd16_be_from_u16(const u16 *p, u16 idx)
{
    return rd16be((const u8*)(p + idx));
}

static inline void wr16_be_to_u16(u16 *p, u16 idx, u16 v)
{
    wr16be((u8*)(p + idx), v);
}

static inline void wr16be(u8 *p, u16 v)
{
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static inline u64 rd64be(const u8 *p)
{
    u64 v = 0;
    for (int i = 0; i < 8; i++) {
        v = (v << 8) | (u64)p[i];
    }
    return v;
}

void __CARDCheckSum(void *ptr, int length, u16 *checksum, u16 *checksumInv)
{
    u8 const *p = (u8 const *)ptr;
    u32 n = (u32)length / 2u;
    u16 cs = 0;
    u16 csi = 0;
    for (u32 i = 0; i < n; i++) {
        u16 v = (u16)(((u16)p[i * 2u] << 8) | (u16)p[i * 2u + 1u]);
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
    *checksumInv = csi;
}

static inline int card_is_valid_block_no(const GcCardControl *card, u16 iBlock)
{
    return (CARD_NUM_SYSTEM_BLOCK <= iBlock && iBlock < card->cblock);
}

static s32 VerifyID(GcCardControl *card)
{
    const u8 *id = (const u8 *)(uintptr_t)card->work_area;
    if (!id) {
        return CARD_RESULT_BROKEN;
    }

    u16 deviceID = rd16be(id + 32u);
    u16 size = rd16be(id + 34u);
    u16 encode = rd16be(id + 36u);
    if (deviceID != 0 || size != card->size_u16) {
        return CARD_RESULT_BROKEN;
    }

    u16 checksum = 0, checksumInv = 0;
    __CARDCheckSum((void *)id, 512 - 4, &checksum, &checksumInv);
    if (rd16be(id + 508u) != checksum || rd16be(id + 510u) != checksumInv) {
        return CARD_RESULT_BROKEN;
    }

    if (encode != OSGetFontEncode()) {
        return CARD_RESULT_ENCODING;
    }

    u64 rand = rd64be(id + 12u);
    int chan = (int)(card - gc_card_block);
    OSSramEx *sramEx = __OSLockSramEx();
    if (!sramEx) {
        return CARD_RESULT_BROKEN;
    }
    for (int i = 0; i < 12; i++) {
        rand = (rand * 1103515245ull + 12345ull) >> 16;
        if (id[i] != (u8)(sramEx->flashID[chan][i] + (u8)rand)) {
            (void)__OSUnlockSramEx(0);
            return CARD_RESULT_BROKEN;
        }
        rand = ((rand * 1103515245ull + 12345ull) >> 16) & 0x7FFFull;
    }
    (void)__OSUnlockSramEx(0);
    return CARD_RESULT_READY;
}

static s32 VerifyDir(GcCardControl *card, int *outCurrent)
{
    u8 *wa = (u8 *)(uintptr_t)card->work_area;
    u8 *dir[2];
    u8 *chk[2];
    u16 checkSum, checkSumInv;
    int errors = 0;
    int current = 0;

    if (outCurrent) {
        *outCurrent = 0;
    }

    for (int i = 0; i < 2; i++) {
        dir[i] = wa + (u32)(1 + i) * CARD_SYSTEM_BLOCK_SIZE;
        chk[i] = dir[i] + (u32)CARD_MAX_FILE * CARD_DIR_SIZE;
        __CARDCheckSum(dir[i], (int)(CARD_SYSTEM_BLOCK_SIZE - 4u), &checkSum, &checkSumInv);

        u16 got_cs = rd16be(chk[i] + (CARD_DIR_SIZE - 4));
        u16 got_csi = rd16be(chk[i] + (CARD_DIR_SIZE - 2));
        if (got_cs != checkSum || got_csi != checkSumInv) {
            errors++;
            current = i;
            card->current_dir_ptr = 0u;
        }
    }

    if (errors == 0) {
        if (card->current_dir_ptr == 0u) {
            int16_t cc0 = (int16_t)rd16be(chk[0] + (CARD_DIR_SIZE - 6));
            int16_t cc1 = (int16_t)rd16be(chk[1] + (CARD_DIR_SIZE - 6));
            current = ((cc0 - cc1) < 0) ? 0 : 1;
            card->current_dir_ptr = (uintptr_t)dir[current];
            memcpy(dir[current], dir[current ^ 1], CARD_SYSTEM_BLOCK_SIZE);
        }
        else {
            current = (card->current_dir_ptr == (uintptr_t)dir[0]) ? 0 : 1;
        }
    }
    if (outCurrent) {
        *outCurrent = current;
    }
    return (s32)errors;
}

static s32 VerifyFAT(GcCardControl *card, int *outCurrent)
{
    u8 *wa = (u8 *)(uintptr_t)card->work_area;
    u8 *fat[2];
    u16 checkSum, checkSumInv;
    int errors = 0;
    int current = 0;

    if (outCurrent) {
        *outCurrent = 0;
    }

    for (int i = 0; i < 2; i++) {
        fat[i] = wa + (u32)(3 + i) * CARD_SYSTEM_BLOCK_SIZE;
        const u8 *fat_check = fat[i] + (u32)CARD_FAT_CHECKCODE * 2u;
        __CARDCheckSum((void *)fat_check, (int)(CARD_SYSTEM_BLOCK_SIZE - 4u), &checkSum, &checkSumInv);

        u16 got_cs = rd16be(fat[i] + (u32)CARD_FAT_CHECKSUM * 2u);
        u16 got_csi = rd16be(fat[i] + (u32)CARD_FAT_CHECKSUMINV * 2u);
        if (got_cs != checkSum || got_csi != checkSumInv) {
            errors++;
            current = i;
            card->current_fat_ptr = 0u;
            continue;
        }

        u16 cFree = 0;
        for (u16 nBlock = CARD_NUM_SYSTEM_BLOCK; nBlock < (u16)card->cblock; nBlock++) {
            if (rd16be(fat[i] + (u32)nBlock * 2u) == CARD_FAT_AVAIL) {
                cFree++;
            }
        }
        if (cFree != rd16be(fat[i] + (u32)CARD_FAT_FREEBLOCKS * 2u)) {
            errors++;
            current = i;
            card->current_fat_ptr = 0u;
        }
    }

    if (errors == 0) {
        if (card->current_fat_ptr == 0u) {
            int16_t cc0 = (int16_t)rd16be(fat[0] + (u32)CARD_FAT_CHECKCODE * 2u);
            int16_t cc1 = (int16_t)rd16be(fat[1] + (u32)CARD_FAT_CHECKCODE * 2u);
            current = ((cc0 - cc1) < 0) ? 0 : 1;
            card->current_fat_ptr = (uintptr_t)fat[current];
            memcpy(fat[current], fat[current ^ 1], CARD_SYSTEM_BLOCK_SIZE);
        }
        else {
            current = (card->current_fat_ptr == (uintptr_t)fat[0]) ? 0 : 1;
        }
    }
    if (outCurrent) {
        *outCurrent = current;
    }
    return (s32)errors;
}

s32 __CARDVerify(GcCardControl *card)
{
    if (!card) {
        return CARD_RESULT_FATAL_ERROR;
    }

    s32 result = VerifyID(card);
    if (result < 0) {
        return result;
    }

    int errors = 0;
    errors += (int)VerifyDir(card, NULL);
    errors += (int)VerifyFAT(card, NULL);
    switch (errors) {
        case 0:
            return CARD_RESULT_READY;
        case 1:
        default:
            return CARD_RESULT_BROKEN;
    }
}

s32 CARDCheckExAsync(s32 chan, s32 *xferBytes, CARDCallback callback)
{
    GcCardControl *card;
    u8 *wa;
    u16 *dir[2];
    u16 *fat[2];
    u16 *map;
    s32 result;
    int errors;
    int currentFat = 0;
    int currentDir = 0;
    s32 fileNo;
    u16 iBlock;
    u16 cBlock;
    u16 cFree;
    int updateFat = 0;
    int updateDir = 0;
    int updateOrphan = 0;
    int valid;

    if (xferBytes) {
        *xferBytes = 0;
    }
    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    result = VerifyID(card);
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }

    errors = VerifyDir(card, &currentDir);
    errors += VerifyFAT(card, &currentFat);
    if (errors > 1) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }

    wa = (u8 *)(uintptr_t)card->work_area;
    dir[0] = (u16 *)(wa + (u32)(1 + 0) * CARD_SYSTEM_BLOCK_SIZE);
    dir[1] = (u16 *)(wa + (u32)(1 + 1) * CARD_SYSTEM_BLOCK_SIZE);
    fat[0] = (u16 *)(wa + (u32)(3 + 0) * CARD_SYSTEM_BLOCK_SIZE);
    fat[1] = (u16 *)(wa + (u32)(3 + 1) * CARD_SYSTEM_BLOCK_SIZE);

    switch (errors) {
        case 0:
            break;
        case 1:
            if (!card->current_dir_ptr) {
                card->current_dir_ptr = (uintptr_t)dir[currentDir];
                memcpy(dir[currentDir], dir[currentDir ^ 1], CARD_SYSTEM_BLOCK_SIZE);
                updateDir = 1;
            }
            else {
                card->current_fat_ptr = (uintptr_t)fat[currentFat];
                memcpy(fat[currentFat], fat[currentFat ^ 1], CARD_SYSTEM_BLOCK_SIZE);
                updateFat = 1;
            }
            break;
    }

    map = fat[currentFat ^ 1];
    memset(map, 0, CARD_SYSTEM_BLOCK_SIZE);

    valid = (int)card->current_dir_ptr;
    for (fileNo = 0; fileNo < CARD_MAX_FILE; fileNo++) {
        u8 *ent = (u8 *)((uintptr_t)dir[0] + (uintptr_t)fileNo * CARD_DIR_SIZE);
        u32 entAddr = (u32)(uintptr_t)ent;
        u16 entStartBlock;
        u16 entLength;
        if (!valid) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }

        if (ent[PORT_CARD_DIR_OFF_GAMENAME] == 0xFFu) {
            continue;
        }

        entStartBlock = rd16be((u8 *)(uintptr_t)(entAddr + PORT_CARD_DIR_OFF_STARTBLOCK));
        entLength = rd16be((u8 *)(uintptr_t)(entAddr + PORT_CARD_DIR_OFF_LENGTH));
        for (iBlock = entStartBlock, cBlock = 0; iBlock != 0xFFFFu && cBlock < entLength; iBlock = rd16_be_from_u16(fat[currentFat], iBlock), ++cBlock) {
            if (!card_is_valid_block_no(card, iBlock) || 1 < ++map[iBlock]) {
                return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
            }
        }
        if (cBlock != entLength || iBlock != 0xFFFFu) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
    }

    cFree = 0;
    for (iBlock = CARD_NUM_SYSTEM_BLOCK; iBlock < (u16)card->cblock; iBlock++) {
        u16 nextBlock = rd16_be_from_u16(fat[currentFat], iBlock);
        if (map[iBlock] == 0) {
            if (nextBlock != CARD_FAT_AVAIL) {
                wr16_be_to_u16(fat[currentFat], iBlock, CARD_FAT_AVAIL);
                updateOrphan = 1;
            }
            cFree++;
        }
        else if (!card_is_valid_block_no(card, nextBlock) && nextBlock != 0xFFFFu) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
    }
    if (cFree != rd16_be_from_u16(fat[currentFat], CARD_FAT_FREEBLOCKS)) {
        wr16_be_to_u16(fat[currentFat], CARD_FAT_FREEBLOCKS, cFree);
        updateOrphan = 1;
    }
    if (updateOrphan) {
        // __CARDCheckSum computes u16 checksums over a BE byte stream. On PPC,
        // passing pointers into the FAT block works because the CPU is BE.
        // On host, we must store the checksum fields back as BE bytes.
        u16 cs = 0;
        u16 csi = 0;
        __CARDCheckSum((void *)&fat[currentFat][CARD_FAT_CHECKCODE], CARD_SYSTEM_BLOCK_SIZE - (int)sizeof(u32), &cs, &csi);
        wr16_be_to_u16(fat[currentFat], CARD_FAT_CHECKSUM, cs);
        wr16_be_to_u16(fat[currentFat], CARD_FAT_CHECKSUMINV, csi);
    }

    memcpy(fat[currentFat ^ 1], fat[currentFat], CARD_SYSTEM_BLOCK_SIZE);

    if (updateDir) {
        if (xferBytes) {
            *xferBytes = CARD_SYSTEM_BLOCK_SIZE;
        }
        return __CARDUpdateDir(chan, callback);
    }

    if (updateFat | updateOrphan) {
        if (xferBytes) {
            *xferBytes = CARD_SYSTEM_BLOCK_SIZE;
        }
        return __CARDUpdateFatBlock(chan, fat[currentFat], callback);
    }

    __CARDPutControlBlock(card, CARD_RESULT_READY);
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    return CARD_RESULT_READY;
}

s32 CARDCheckAsync(s32 chan, CARDCallback callback)
{
    s32 xferBytes;
    (void)xferBytes;
    return CARDCheckExAsync(chan, &xferBytes, callback);
}

s32 CARDCheck(s32 chan)
{
    s32 xferBytes;
    s32 result = CARDCheckExAsync(chan, &xferBytes, __CARDSyncCallback);
    if (result >= 0) {
        s32 sync = __CARDSync(chan);
        return sync;
    }
    return result;
}
