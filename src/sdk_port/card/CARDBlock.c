#include <stdint.h>
#include <string.h>

#include "card_bios.h"
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
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_INSSPACE = -9,
    CARD_RESULT_FATAL_ERROR = -128,
};

enum { CARD_NUM_SYSTEM_BLOCK = 5u };
enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };

static inline u16 rd16be(const u8* p)
{
    return (u16)(((u16)p[0] << 8u) | (u16)p[1]);
}

static inline void wr16be(u8* p, u16 v)
{
    p[0] = (u8)(v >> 8u);
    p[1] = (u8)v;
}

static inline int card_is_valid_block_no(const GcCardControl* card, u16 iBlock)
{
    return ((u16)CARD_NUM_SYSTEM_BLOCK <= iBlock && (u32)iBlock < (u32)card->cblock);
}

// __CARDCheckSum expects a BE byte stream, matching PPC memory layout.
void __CARDCheckSum(void* ptr, int length, u16* checksum, u16* checksumInv);

static inline u16 fat_get(const u8* fat, u16 idx)
{
    return rd16be(fat + (u32)idx * 2u);
}

static inline void fat_set(u8* fat, u16 idx, u16 v)
{
    wr16be(fat + (u32)idx * 2u, v);
}

u16* __CARDGetFatBlock(GcCardControl* card)
{
    if (!card || !card->current_fat_ptr) {
        return 0;
    }
    return (u16*)card_ptr(card->current_fat_ptr, CARD_SYSTEM_BLOCK_SIZE);
}

// Host-port note:
// The real SDK persists FAT updates via erase+write I/O. For deterministic unit tests,
// we model FAT updates as immediate (checksum + counter update), with no EXI I/O.
s32 __CARDUpdateFatBlock(s32 chan, u16* fat_u16, CARDCallback callback)
{
    if (chan < 0 || chan >= 2) {
        return CARD_RESULT_FATAL_ERROR;
    }

    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) {
        return CARD_RESULT_NOCARD;
    }

    u8* fat = (u8*)fat_u16;
    if (!fat) {
        return CARD_RESULT_BROKEN;
    }

    // Increment checkcode (u16 BE at index PORT_CARD_FAT_CHECKCODE).
    u16 cc = fat_get(fat, (u16)PORT_CARD_FAT_CHECKCODE);
    cc = (u16)(cc + 1u);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKCODE, cc);

    // Checksum over bytes starting at checkcode, length 0x1FFC (8192 - 4).
    u16 cs = 0, csi = 0;
    __CARDCheckSum(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUM, cs);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUMINV, csi);

    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    return CARD_RESULT_READY;
}

s32 __CARDAllocBlock(s32 chan, u32 cBlock, CARDCallback callback)
{
    if (chan < 0 || chan >= 2) {
        return CARD_RESULT_FATAL_ERROR;
    }

    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) {
        return CARD_RESULT_NOCARD;
    }

    u8* fat = (u8*)__CARDGetFatBlock(card);
    if (!fat) {
        return CARD_RESULT_BROKEN;
    }

    u16 freeBlocks = fat_get(fat, (u16)PORT_CARD_FAT_FREEBLOCKS);
    if (freeBlocks < (u16)cBlock) {
        return CARD_RESULT_INSSPACE;
    }
    fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, (u16)(freeBlocks - (u16)cBlock));

    u16 startBlock = 0xFFFFu;
    u16 iBlock = fat_get(fat, (u16)PORT_CARD_FAT_LASTSLOT);
    u16 prevBlock = 0u;
    u16 count = 0u;

    while (cBlock > 0u) {
        if ((u32)card->cblock - (u32)CARD_NUM_SYSTEM_BLOCK < (u32)(++count)) {
            return CARD_RESULT_BROKEN;
        }

        iBlock++;
        if (iBlock < (u16)CARD_NUM_SYSTEM_BLOCK || (u32)card->cblock <= (u32)iBlock) {
            iBlock = (u16)CARD_NUM_SYSTEM_BLOCK;
        }

        if (fat_get(fat, iBlock) == PORT_CARD_FAT_AVAIL) {
            if (startBlock == 0xFFFFu) {
                startBlock = iBlock;
            } else {
                fat_set(fat, prevBlock, iBlock);
            }
            prevBlock = iBlock;
            fat_set(fat, iBlock, 0xFFFFu);
            cBlock--;
        }
    }

    fat_set(fat, (u16)PORT_CARD_FAT_LASTSLOT, iBlock);
    card->startBlock = startBlock;

    return __CARDUpdateFatBlock(chan, (u16*)fat, callback);
}

s32 __CARDFreeBlock(s32 chan, u16 nBlock, CARDCallback callback)
{
    if (chan < 0 || chan >= 2) {
        return CARD_RESULT_FATAL_ERROR;
    }

    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) {
        return CARD_RESULT_NOCARD;
    }

    u8* fat = (u8*)__CARDGetFatBlock(card);
    if (!fat) {
        return CARD_RESULT_BROKEN;
    }

    while (nBlock != 0xFFFFu) {
        if (!card_is_valid_block_no(card, nBlock)) {
            return CARD_RESULT_BROKEN;
        }
        u16 next = fat_get(fat, nBlock);
        fat_set(fat, nBlock, PORT_CARD_FAT_AVAIL);

        u16 fb = fat_get(fat, (u16)PORT_CARD_FAT_FREEBLOCKS);
        fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, (u16)(fb + 1u));

        nBlock = next;
    }

    return __CARDUpdateFatBlock(chan, (u16*)fat, callback);
}

