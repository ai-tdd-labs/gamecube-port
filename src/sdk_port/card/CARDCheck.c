#include <stdint.h>
#include <string.h>

#include "card_bios.h"
#include "dolphin/OSRtcPriv.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_ENCODING = -13,
    CARD_RESULT_FATAL_ERROR = -128,
};

enum { CARD_SYSTEM_BLOCK_SIZE = 8 * 1024 };
enum { CARD_NUM_SYSTEM_BLOCK = 5 };
enum { CARD_MAX_FILE = 127 };
enum { CARD_DIR_SIZE = 64 }; // sizeof(CARDDir) from SDK (MP4)

enum {
    CARD_FAT_AVAIL = 0x0000u,
    CARD_FAT_CHECKSUM = 0x0000u,
    CARD_FAT_CHECKSUMINV = 0x0001u,
    CARD_FAT_CHECKCODE = 0x0002u,
    CARD_FAT_FREEBLOCKS = 0x0003u,
};

extern u16 OSGetFontEncode(void);

static inline u16 rd16be(const u8 *p)
{
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
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

static void card_checksum_u16be(const u8 *ptr, u32 length, u16 *checksum, u16 *checksumInv)
{
    u32 n = length / 2u;
    u16 cs = 0;
    u16 csi = 0;
    for (u32 i = 0; i < n; i++) {
        u16 v = rd16be(ptr + i * 2u);
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
    card_checksum_u16be(id, 512u - 4u, &checksum, &checksumInv);
    u16 want_cs = rd16be(id + 508u);
    u16 want_csi = rd16be(id + 510u);
    if (want_cs != checksum || want_csi != checksumInv) {
        return CARD_RESULT_BROKEN;
    }

    if (encode != OSGetFontEncode()) {
        return CARD_RESULT_ENCODING;
    }

    // Compare flashID-derived serial bytes.
    u64 rand = rd64be(id + 12u);
    int chan = (int)(card - gc_card_block);
    OSSramEx *sramEx = __OSLockSramEx();
    if (!sramEx) {
        return CARD_RESULT_BROKEN;
    }
    for (int i = 0; i < 12; i++) {
        rand = (rand * 1103515245ull + 12345ull) >> 16;
        u8 want = (u8)(sramEx->flashID[chan][i] + (u8)rand);
        if (id[i] != want) {
            (void)__OSUnlockSramEx(0);
            return CARD_RESULT_BROKEN;
        }
        rand = ((rand * 1103515245ull + 12345ull) >> 16) & 0x7FFFull;
    }
    (void)__OSUnlockSramEx(0);
    return CARD_RESULT_READY;
}

static s32 VerifyDir(GcCardControl *card)
{
    u8 *wa = (u8 *)(uintptr_t)card->work_area;
    u8 *dir[2];
    u8 *chk[2];
    u16 checkSum, checkSumInv;
    int errors = 0;
    int current = 0;

    for (int i = 0; i < 2; i++) {
        dir[i] = wa + (u32)(1 + i) * CARD_SYSTEM_BLOCK_SIZE;
        chk[i] = dir[i] + (u32)CARD_MAX_FILE * CARD_DIR_SIZE;
        card_checksum_u16be(dir[i], (u32)CARD_SYSTEM_BLOCK_SIZE - 4u, &checkSum, &checkSumInv);

        u16 got_cs = rd16be(chk[i] + (CARD_DIR_SIZE - 4));
        u16 got_csi = rd16be(chk[i] + (CARD_DIR_SIZE - 2));
        if (got_cs != checkSum || got_csi != checkSumInv) {
            errors++;
            current = i;
            card->current_dir_ptr = 0;
        }
    }

    if (errors == 0) {
        if (card->current_dir_ptr == 0) {
            int16_t cc0 = (int16_t)rd16be(chk[0] + (CARD_DIR_SIZE - 6));
            int16_t cc1 = (int16_t)rd16be(chk[1] + (CARD_DIR_SIZE - 6));
            current = ((cc0 - cc1) < 0) ? 0 : 1;
            card->current_dir_ptr = (uintptr_t)dir[current];
            memcpy(dir[current], dir[current ^ 1], CARD_SYSTEM_BLOCK_SIZE);
        } else {
            current = (card->current_dir_ptr == (uintptr_t)dir[0]) ? 0 : 1;
        }
    }
    (void)current;
    return (s32)errors;
}

static s32 VerifyFAT(GcCardControl *card)
{
    u8 *wa = (u8 *)(uintptr_t)card->work_area;
    u8 *fat[2];
    u16 checkSum, checkSumInv;
    int errors = 0;
    int current = 0;

    for (int i = 0; i < 2; i++) {
        fat[i] = wa + (u32)(3 + i) * CARD_SYSTEM_BLOCK_SIZE;
        const u8 *fat_check = fat[i] + (u32)CARD_FAT_CHECKCODE * 2u;
        card_checksum_u16be(fat_check, (u32)CARD_SYSTEM_BLOCK_SIZE - 4u, &checkSum, &checkSumInv);
        u16 got_cs = rd16be(fat[i] + (u32)CARD_FAT_CHECKSUM * 2u);
        u16 got_csi = rd16be(fat[i] + (u32)CARD_FAT_CHECKSUMINV * 2u);
        if (got_cs != checkSum || got_csi != checkSumInv) {
            errors++;
            current = i;
            card->current_fat_ptr = 0;
            continue;
        }

        u16 cFree = 0;
        for (u16 nBlock = CARD_NUM_SYSTEM_BLOCK; nBlock < (u16)card->cblock; nBlock++) {
            u16 v = rd16be(fat[i] + (u32)nBlock * 2u);
            if (v == CARD_FAT_AVAIL) {
                cFree++;
            }
        }
        u16 want_free = rd16be(fat[i] + (u32)CARD_FAT_FREEBLOCKS * 2u);
        if (cFree != want_free) {
            errors++;
            current = i;
            card->current_fat_ptr = 0;
            continue;
        }
    }

    if (errors == 0) {
        if (card->current_fat_ptr == 0) {
            int16_t cc0 = (int16_t)rd16be(fat[0] + (u32)CARD_FAT_CHECKCODE * 2u);
            int16_t cc1 = (int16_t)rd16be(fat[1] + (u32)CARD_FAT_CHECKCODE * 2u);
            current = ((cc0 - cc1) < 0) ? 0 : 1;
            card->current_fat_ptr = (uintptr_t)fat[current];
            memcpy(fat[current], fat[current ^ 1], CARD_SYSTEM_BLOCK_SIZE);
        } else {
            current = (card->current_fat_ptr == (uintptr_t)fat[0]) ? 0 : 1;
        }
    }
    (void)current;
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
    errors += (int)VerifyDir(card);
    errors += (int)VerifyFAT(card);
    switch (errors) {
        case 0:
            return CARD_RESULT_READY;
        case 1:
        default:
            return CARD_RESULT_BROKEN;
    }
}
