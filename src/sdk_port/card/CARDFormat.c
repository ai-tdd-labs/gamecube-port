#include <stdint.h>
#include <string.h>

#include "card_bios.h"
#include "card_dir.h"
#include "card_fat.h"
#include "card_mem.h"

#include "dolphin/OSRtcPriv.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;

typedef void (*CARDCallback)(s32 chan, s32 result);

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_FATAL_ERROR = -128,
};

enum { CARD_NUM_SYSTEM_BLOCK = 5u };
enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };
enum { TICKS_PER_SEC = (162000000u / 4u) };

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

static inline void wr64be(u8* p, u64 v)
{
    for (int i = 7; i >= 0; i--) {
        p[7 - i] = (u8)(v >> (u32)(i * 8));
    }
}

static u64 oracle_time_ticks(void)
{
    static u64 ticks;
    ticks += (u64)TICKS_PER_SEC;
    return ticks;
}

// Internal helpers.
s32 __CARDGetControlBlock(s32 chan, GcCardControl** pcard);
s32 __CARDPutControlBlock(GcCardControl* card, s32 result);
u16 __CARDGetFontEncode(void);
void __CARDCheckSum(void* ptr, int length, u16* checksum, u16* checksumInv);

static void init_dir_block(u8* dir, u16 checkCode)
{
    // Entire system block is 8KiB.
    memset(dir, 0xFFu, CARD_SYSTEM_BLOCK_SIZE);

    // Dir check struct is the last 64 bytes in the block.
    u8* chk = dir + (u32)PORT_CARD_MAX_FILE * (u32)PORT_CARD_DIR_SIZE;
    wr16be(chk + (PORT_CARD_DIR_SIZE - 6u), checkCode);

    u16 cs = 0, csi = 0;
    __CARDCheckSum(dir, (int)(CARD_SYSTEM_BLOCK_SIZE - 4u), &cs, &csi);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 4u), cs);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 2u), csi);
}

static void init_fat_block(u8* fat, u16 checkCode, u16 cBlock)
{
    memset(fat, 0x00u, CARD_SYSTEM_BLOCK_SIZE);

    // u16 entries are BE in memory.
    wr16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, checkCode);
    wr16be(fat + (u32)PORT_CARD_FAT_FREEBLOCKS * 2u, (u16)(cBlock - (u16)CARD_NUM_SYSTEM_BLOCK));
    wr16be(fat + (u32)PORT_CARD_FAT_LASTSLOT * 2u, (u16)((u16)CARD_NUM_SYSTEM_BLOCK - 1u));

    u16 cs = 0, csi = 0;
    __CARDCheckSum(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
    wr16be(fat + (u32)PORT_CARD_FAT_CHECKSUM * 2u, cs);
    wr16be(fat + (u32)PORT_CARD_FAT_CHECKSUMINV * 2u, csi);
}

s32 __CARDFormatRegionAsync(s32 chan, u16 encode, CARDCallback callback)
{
    GcCardControl* card;
    s32 result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    u8* work = (u8*)card_ptr(card->work_area, (size_t)CARD_NUM_SYSTEM_BLOCK * (size_t)CARD_SYSTEM_BLOCK_SIZE);
    if (!work) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }

    // Block0: CARDID lives in the first 512 bytes.
    memset(work, 0xFFu, CARD_SYSTEM_BLOCK_SIZE);
    u8* id = work;

    // Populate serial area based on SRAM/SRAMEx.
    OSSram* sram = __OSLockSram();
    if (!sram) {
        __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
        return CARD_RESULT_FATAL_ERROR;
    }
    // serial[20] counterBias (BE)
    wr32be(id + 20u, sram->counterBias);
    // serial[24] language (BE)
    wr32be(id + 24u, (u32)sram->language);
    (void)__OSUnlockSram(FALSE);

    u64 time = oracle_time_ticks();
    u64 rand = time;

    OSSramEx* sramEx = __OSLockSramEx();
    if (!sramEx) {
        __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
        return CARD_RESULT_FATAL_ERROR;
    }
    for (int i = 0; i < 12; i++) {
        rand = (rand * 1103515245u + 12345u) >> 16;
        id[(u32)i] = (u8)(sramEx->flashID[chan][(u32)i] + (u8)rand);
        rand = ((rand * 1103515245u + 12345u) >> 16) & 0x7FFFull;
    }
    (void)__OSUnlockSramEx(FALSE);

    // serial[28] viDTVStatus (we model as 0 for determinism).
    wr32be(id + 28u, 0u);
    // serial[12] time (BE u64)
    wr64be(id + 12u, time);

    // deviceID=0 (BE u16), size, encode (BE u16 each).
    wr16be(id + 32u, 0u);
    wr16be(id + 34u, card->size_u16);
    wr16be(id + 36u, encode);

    // Header checksum over first 512 bytes minus final u32 (4 bytes).
    u16 id_cs = 0, id_csi = 0;
    __CARDCheckSum(id, 512 - 4, &id_cs, &id_csi);
    wr16be(id + 508u, id_cs);
    wr16be(id + 510u, id_csi);

    // Blocks1/2: directories.
    init_dir_block(work + (1u + 0u) * (u32)CARD_SYSTEM_BLOCK_SIZE, 0);
    init_dir_block(work + (1u + 1u) * (u32)CARD_SYSTEM_BLOCK_SIZE, 1);

    // Blocks3/4: FATs.
    init_fat_block(work + (3u + 0u) * (u32)CARD_SYSTEM_BLOCK_SIZE, 0, (u16)card->cblock);
    init_fat_block(work + (3u + 1u) * (u32)CARD_SYSTEM_BLOCK_SIZE, 1, (u16)card->cblock);

    // Deterministic host port: we do not model the erase+write I/O loop here.
    // Format is treated as immediate: system blocks are prepared in work_area,
    // and current_dir_ptr/current_fat_ptr are installed right away.
    card->current_dir_ptr = (uintptr_t)(work + (1u + 0u) * (u32)CARD_SYSTEM_BLOCK_SIZE);
    memcpy((void*)card->current_dir_ptr, work + (1u + 1u) * (u32)CARD_SYSTEM_BLOCK_SIZE, CARD_SYSTEM_BLOCK_SIZE);

    card->current_fat_ptr = (uintptr_t)(work + (3u + 0u) * (u32)CARD_SYSTEM_BLOCK_SIZE);
    memcpy((void*)card->current_fat_ptr, work + (3u + 1u) * (u32)CARD_SYSTEM_BLOCK_SIZE, CARD_SYSTEM_BLOCK_SIZE);

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    CARDCallback cb = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;
    __CARDPutControlBlock(card, CARD_RESULT_READY);
    if (cb) {
        cb(chan, CARD_RESULT_READY);
    }
    return CARD_RESULT_READY;
}

s32 CARDFormatAsync(s32 chan, CARDCallback callback)
{
    return __CARDFormatRegionAsync(chan, __CARDGetFontEncode(), callback);
}

s32 CARDFormat(s32 chan)
{
    s32 result = __CARDFormatRegionAsync(chan, __CARDGetFontEncode(), __CARDSyncCallback);
    if (result < 0) {
        return result;
    }
    return __CARDSync(chan);
}
