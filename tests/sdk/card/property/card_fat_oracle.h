/*
 * card_fat_oracle.h --- Oracle for CARD FAT property tests.
 *
 * Exact copy of decomp __CARDAllocBlock, __CARDFreeBlock, __CARDCheckSum
 * with oracle_ prefix.  Hardware callbacks (DCStoreRange, __CARDEraseSector,
 * __CARDWrite) are stripped --- UpdateFatBlock only does checksum + counter.
 *
 * Source of truth:
 *   external/mp4-decomp/src/dolphin/card/CARDBlock.c
 *   external/mp4-decomp/src/dolphin/card/CARDCheck.c
 *   external/mp4-decomp/include/dolphin/CARDPriv.h
 */
#pragma once

#include <stdint.h>
#include <string.h>

/* Oracle strictness tier for reporting/gating. */
#define ORACLE_TIER "DECOMP_ADAPTED"

/* ── Types matching the decomp ── */
typedef uint16_t oracle_u16;
typedef uint32_t oracle_u32;
typedef int32_t  oracle_s32;

/* ── FAT array index constants (CARDPriv.h) ── */
#define ORACLE_CARD_FAT_AVAIL       0x0000u
#define ORACLE_CARD_FAT_CHECKSUM    0   /* fat[0] */
#define ORACLE_CARD_FAT_CHECKSUMINV 1   /* fat[1] */
#define ORACLE_CARD_FAT_CHECKCODE   2   /* fat[2] = update counter */
#define ORACLE_CARD_FAT_FREEBLOCKS  3   /* fat[3] = free block count */
#define ORACLE_CARD_FAT_LASTSLOT    4   /* fat[4] = last-alloc hint */
#define ORACLE_CARD_NUM_SYSTEM_BLOCK 5

/* ── FAT size: always 8KB = 4096 u16 entries ── */
#define ORACLE_CARD_FAT_ENTRIES     4096
#define ORACLE_CARD_FAT_BYTES       (ORACLE_CARD_FAT_ENTRIES * 2)

/* ── Result codes (card.h) ── */
#define ORACLE_CARD_RESULT_READY     0
#define ORACLE_CARD_RESULT_NOCARD   -3
#define ORACLE_CARD_RESULT_BROKEN   -6
#define ORACLE_CARD_RESULT_INSSPACE -9

/* ── Minimal CARDControl: only fields used by AllocBlock/FreeBlock ── */
typedef struct {
    int         attached;
    oracle_u16  cBlock;      /* total blocks (5 system + data) */
    oracle_u16  startBlock;  /* output: first block of new chain */
    oracle_u16 *currentFat;  /* FAT array (4096 u16 entries) */
} oracle_CARDControl;

#define oracle_CARDIsValidBlockNo(card, iBlock) \
    (ORACLE_CARD_NUM_SYSTEM_BLOCK <= (iBlock) && (iBlock) < (card)->cBlock)

/* ────────────────────────────────────────────────────────────────────
 * __CARDCheckSum  (CARDCheck.c:11-28, exact copy)
 * ──────────────────────────────────────────────────────────────────── */
static void oracle_CARDCheckSum(void *ptr, int length,
                                oracle_u16 *checksum, oracle_u16 *checksumInv)
{
    oracle_u16 *p;
    int i;

    length /= sizeof(oracle_u16);
    *checksum = *checksumInv = 0;
    for (i = 0, p = ptr; i < length; i++, p++) {
        *checksum += *p;
        *checksumInv += ~*p;
    }
    if (*checksum == 0xffff) {
        *checksum = 0;
    }
    if (*checksumInv == 0xffff) {
        *checksumInv = 0;
    }
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDGetFatBlock  (CARDBlock.c:10-13, trivial)
 * ──────────────────────────────────────────────────────────────────── */
static oracle_u16 *oracle_CARDGetFatBlock(oracle_CARDControl *card)
{
    return card->currentFat;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDUpdateFatBlock  (CARDBlock.c:159-170, hardware I/O stripped)
 *
 * Keeps: ++fat[2], __CARDCheckSum(fat+2, 0x1FFC, fat, fat+1)
 * Strips: DCStoreRange, __CARDEraseSector, callback plumbing
 * ──────────────────────────────────────────────────────────────────── */
static oracle_s32 oracle_CARDUpdateFatBlock(oracle_CARDControl *card)
{
    oracle_u16 *fat = card->currentFat;
    ++fat[ORACLE_CARD_FAT_CHECKCODE];
    oracle_CARDCheckSum(&fat[ORACLE_CARD_FAT_CHECKCODE], 0x1FFC,
                        &fat[ORACLE_CARD_FAT_CHECKSUM],
                        &fat[ORACLE_CARD_FAT_CHECKSUMINV]);
    return ORACLE_CARD_RESULT_READY;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDAllocBlock  (CARDBlock.c:82-131, exact logic)
 *
 * Changes from decomp:
 *   - Takes oracle_CARDControl* instead of chan index
 *   - No callback parameter (hardware stripped)
 *   - Calls oracle_CARDUpdateFatBlock instead of __CARDUpdateFatBlock
 * ──────────────────────────────────────────────────────────────────── */
static oracle_s32 oracle_CARDAllocBlock(oracle_CARDControl *card, oracle_u32 cBlock)
{
    oracle_u16 *fat;
    oracle_u16 iBlock;
    oracle_u16 startBlock;
    oracle_u16 prevBlock = 0;
    oracle_u16 count;

    if (!card->attached) {
        return ORACLE_CARD_RESULT_NOCARD;
    }

    fat = oracle_CARDGetFatBlock(card);
    if (fat[ORACLE_CARD_FAT_FREEBLOCKS] < cBlock) {
        return ORACLE_CARD_RESULT_INSSPACE;
    }

    fat[ORACLE_CARD_FAT_FREEBLOCKS] -= (oracle_u16)cBlock;
    startBlock = 0xFFFF;
    iBlock = fat[ORACLE_CARD_FAT_LASTSLOT];
    count = 0;
    while (0 < cBlock) {
        if (card->cBlock - ORACLE_CARD_NUM_SYSTEM_BLOCK < ++count) {
            return ORACLE_CARD_RESULT_BROKEN;
        }

        iBlock++;
        if (!oracle_CARDIsValidBlockNo(card, iBlock)) {
            iBlock = ORACLE_CARD_NUM_SYSTEM_BLOCK;
        }

        if (fat[iBlock] == ORACLE_CARD_FAT_AVAIL) {
            if (startBlock == 0xFFFF) {
                startBlock = iBlock;
            }
            else {
                fat[prevBlock] = iBlock;
            }
            prevBlock = iBlock;
            fat[iBlock] = 0xFFFF;
            --cBlock;
        }
    }
    fat[ORACLE_CARD_FAT_LASTSLOT] = iBlock;
    card->startBlock = startBlock;

    return oracle_CARDUpdateFatBlock(card);
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDFreeBlock  (CARDBlock.c:133-157, exact logic)
 *
 * Changes from decomp:
 *   - Takes oracle_CARDControl* instead of chan index
 *   - No callback parameter
 *   - Note: decomp has `card = card = &__CARDBlock[chan];` (sic)
 * ──────────────────────────────────────────────────────────────────── */
static oracle_s32 oracle_CARDFreeBlock(oracle_CARDControl *card, oracle_u16 nBlock)
{
    oracle_u16 *fat;
    oracle_u16 nextBlock;

    if (!card->attached) {
        return ORACLE_CARD_RESULT_NOCARD;
    }

    fat = oracle_CARDGetFatBlock(card);
    while (nBlock != 0xFFFF) {
        if (!oracle_CARDIsValidBlockNo(card, nBlock)) {
            return ORACLE_CARD_RESULT_BROKEN;
        }

        nextBlock = fat[nBlock];
        fat[nBlock] = 0;
        nBlock = nextBlock;
        ++fat[ORACLE_CARD_FAT_FREEBLOCKS];
    }

    return oracle_CARDUpdateFatBlock(card);
}
