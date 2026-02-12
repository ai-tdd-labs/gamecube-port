/*
 * card_dir_oracle.h --- Oracle for CARD directory property tests.
 *
 * Exact copy of decomp __CARDCompareFileName, __CARDAccess, __CARDIsPublic,
 * __CARDGetFileNo, __CARDSeek with oracle_ prefix. Hardware callbacks stripped.
 *
 * Source of truth:
 *   external/mp4-decomp/src/dolphin/card/CARDOpen.c
 *   external/mp4-decomp/src/dolphin/card/CARDRead.c
 *   external/mp4-decomp/include/dolphin/CARDPriv.h
 *   external/mp4-decomp/include/dolphin/card.h
 */
#pragma once

#include <stdint.h>
#include <string.h>

/* ── Constants (card.h / CARDPriv.h) ── */

#define ORACLE_CARD_FILENAME_MAX 32
#define ORACLE_CARD_MAX_FILE     127
#define ORACLE_CARD_NUM_SYSTEM_BLOCK 5
#define ORACLE_CARD_ATTR_PUBLIC  0x04u

#define ORACLE_CARD_RESULT_READY       0
#define ORACLE_CARD_RESULT_NOCARD     -3
#define ORACLE_CARD_RESULT_NOFILE     -4
#define ORACLE_CARD_RESULT_BROKEN     -6
#define ORACLE_CARD_RESULT_NOPERM    -10
#define ORACLE_CARD_RESULT_LIMIT     -11
#define ORACLE_CARD_RESULT_FATAL_ERROR -128

/* ── Types ── */

/* CARDDir (CARDPriv.h:23-42, 64 bytes) */
typedef struct {
    uint8_t  gameName[4];
    uint8_t  company[2];
    uint8_t  _padding0;
    uint8_t  bannerFormat;
    uint8_t  fileName[ORACLE_CARD_FILENAME_MAX];
    uint32_t time;
    uint32_t iconAddr;
    uint16_t iconFormat;
    uint16_t iconSpeed;
    uint8_t  permission;
    uint8_t  copyTimes;
    uint16_t startBlock;
    uint16_t length;
    uint8_t  _padding1[2];
    uint32_t commentAddr;
} oracle_CARDDir;

/* Minimal DVDDiskID for __CARDAccess */
typedef struct {
    uint8_t gameName[4];
    uint8_t company[2];
} oracle_DVDDiskID;

/* Minimal CARDControl for dir operations */
typedef struct {
    int              attached;
    uint16_t         cBlock;
    int32_t          sectorSize;
    oracle_CARDDir  *currentDir;
    uint16_t        *currentFat;
    oracle_DVDDiskID *diskID;
} oracle_CARDDirCtl;

/* CARDFileInfo (card.h:90-98) */
typedef struct {
    int32_t  chan;
    int32_t  fileNo;
    int32_t  offset;
    int32_t  length;
    uint16_t iBlock;
    uint16_t __padding;
} oracle_CARDFileInfo;

/* Special "no disk" sentinel */
static oracle_DVDDiskID oracle_CARDDiskNone;

#define oracle_CARDIsValidBlockNo(card, iBlock) \
    (ORACLE_CARD_NUM_SYSTEM_BLOCK <= (iBlock) && (iBlock) < (card)->cBlock)

#define ORACLE_TRUNC(n, a) (((uint32_t)(n)) & ~((a)-1))

/* ────────────────────────────────────────────────────────────────────
 * __CARDCompareFileName  (CARDOpen.c:8-31, exact copy)
 * ──────────────────────────────────────────────────────────────────── */
static int oracle_CARDCompareFileName(oracle_CARDDir *ent, const char *fileName)
{
    char *entName;
    char c1;
    char c2;
    int n;

    entName = (char *)ent->fileName;
    n = ORACLE_CARD_FILENAME_MAX;
    while (0 <= --n) {
        if ((c1 = *entName++) != (c2 = *fileName++)) {
            return 0; /* FALSE */
        }
        else if (c2 == '\0') {
            return 1; /* TRUE */
        }
    }

    if (*fileName == '\0') {
        return 1; /* TRUE */
    }

    return 0; /* FALSE */
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDAccess  (CARDOpen.c:33-45, exact copy)
 *
 * Checks gameName/company match between diskID and dir entry.
 * If diskID == &oracle_CARDDiskNone, any non-empty entry is accessible.
 * ──────────────────────────────────────────────────────────────────── */
static int32_t oracle_CARDAccess(oracle_CARDDirCtl *card, oracle_CARDDir *ent)
{
    if (ent->gameName[0] == 0xFF) {
        return ORACLE_CARD_RESULT_NOFILE;
    }

    if (card->diskID == &oracle_CARDDiskNone
        || (memcmp(ent->gameName, card->diskID->gameName, 4) == 0
            && memcmp(ent->company, card->diskID->company, 2) == 0)) {
        return ORACLE_CARD_RESULT_READY;
    }

    return ORACLE_CARD_RESULT_NOPERM;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDIsPublic  (CARDOpen.c:47-58, exact copy)
 * ──────────────────────────────────────────────────────────────────── */
static int32_t oracle_CARDIsPublic(oracle_CARDDir *ent)
{
    if (ent->gameName[0] == 0xFF) {
        return ORACLE_CARD_RESULT_NOFILE;
    }

    if ((ent->permission & ORACLE_CARD_ATTR_PUBLIC) != 0) {
        return ORACLE_CARD_RESULT_READY;
    }

    return ORACLE_CARD_RESULT_NOPERM;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDGetDirBlock  (CARDDir.c:10-13, trivial)
 * ──────────────────────────────────────────────────────────────────── */
static oracle_CARDDir *oracle_CARDGetDirBlock(oracle_CARDDirCtl *card)
{
    return card->currentDir;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDGetFatBlock  (CARDBlock.c:10-13, trivial)
 * ──────────────────────────────────────────────────────────────────── */
static uint16_t *oracle_CARDGetFatBlock(oracle_CARDDirCtl *card)
{
    return card->currentFat;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDGetFileNo  (CARDOpen.c:60-85, exact logic)
 *
 * Changes from decomp:
 *   - Takes oracle_CARDDirCtl* instead of CARDControl*
 *   - Uses oracle_CARDGetDirBlock
 * ──────────────────────────────────────────────────────────────────── */
static int32_t oracle_CARDGetFileNo(oracle_CARDDirCtl *card, const char *fileName,
                                     int32_t *pfileNo)
{
    oracle_CARDDir *dir;
    oracle_CARDDir *ent;
    int32_t fileNo;
    int32_t result;

    if (!card->attached) {
        return ORACLE_CARD_RESULT_NOCARD;
    }

    dir = oracle_CARDGetDirBlock(card);
    for (fileNo = 0; fileNo < ORACLE_CARD_MAX_FILE; fileNo++) {
        ent = &dir[fileNo];
        result = oracle_CARDAccess(card, ent);
        if (result < 0) {
            continue;
        }
        if (oracle_CARDCompareFileName(ent, fileName)) {
            *pfileNo = fileNo;
            return ORACLE_CARD_RESULT_READY;
        }
    }

    return ORACLE_CARD_RESULT_NOFILE;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDSeek  (CARDRead.c:8-53, exact logic)
 *
 * Changes from decomp:
 *   - Takes oracle_CARDDirCtl* directly (no __CARDGetControlBlock)
 *   - __CARDPutControlBlock returns result directly
 *   - card->fileInfo assignment omitted (not relevant for parity)
 * ──────────────────────────────────────────────────────────────────── */
static int32_t oracle_CARDSeek(oracle_CARDFileInfo *fileInfo, int32_t length,
                                int32_t offset, oracle_CARDDirCtl *card)
{
    oracle_CARDDir *dir;
    oracle_CARDDir *ent;
    uint16_t *fat;

    /* __CARDGetControlBlock mock */
    if (!card->attached) {
        return ORACLE_CARD_RESULT_NOCARD;
    }

    if (!oracle_CARDIsValidBlockNo(card, fileInfo->iBlock) ||
        card->cBlock * card->sectorSize <= fileInfo->offset) {
        return ORACLE_CARD_RESULT_FATAL_ERROR;
    }

    dir = oracle_CARDGetDirBlock(card);
    ent = &dir[fileInfo->fileNo];
    if (ent->length * card->sectorSize <= offset ||
        ent->length * card->sectorSize < offset + length) {
        return ORACLE_CARD_RESULT_LIMIT;
    }

    /* card->fileInfo = fileInfo; -- omitted */
    fileInfo->length = length;
    if (offset < fileInfo->offset) {
        fileInfo->offset = 0;
        fileInfo->iBlock = ent->startBlock;
        if (!oracle_CARDIsValidBlockNo(card, fileInfo->iBlock)) {
            return ORACLE_CARD_RESULT_BROKEN;
        }
    }
    fat = oracle_CARDGetFatBlock(card);
    while (fileInfo->offset < (int32_t)ORACLE_TRUNC(offset, card->sectorSize)) {
        fileInfo->offset += card->sectorSize;
        fileInfo->iBlock = fat[fileInfo->iBlock];
        if (!oracle_CARDIsValidBlockNo(card, fileInfo->iBlock)) {
            return ORACLE_CARD_RESULT_BROKEN;
        }
    }

    fileInfo->offset = offset;
    return ORACLE_CARD_RESULT_READY;
}
