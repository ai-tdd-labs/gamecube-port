/*
 * sdk_port/card/card_dir.c --- Host-side port of GC SDK CARD directory ops.
 *
 * Source of truth:
 *   external/mp4-decomp/src/dolphin/card/CARDOpen.c  (__CARDCompareFileName,
 *       __CARDAccess, __CARDIsPublic, __CARDGetFileNo)
 *   external/mp4-decomp/src/dolphin/card/CARDRead.c  (__CARDSeek)
 *
 * Same logic as decomp but CARDDir entries and FAT live in gc_mem
 * (big-endian layout). Hardware callbacks stripped; __CARDGetControlBlock
 * and __CARDPutControlBlock are not needed (card passed directly).
 */
#include <stdint.h>
#include <string.h>
#include "card_dir.h"
#include "../gc_mem.h"

/* ── gc_mem read helpers ── */

static inline uint8_t load_u8(uint32_t addr)
{
    uint8_t *p = gc_mem_ptr(addr, 1);
    return *p;
}

static inline uint16_t load_u16be(uint32_t addr)
{
    uint8_t *p = gc_mem_ptr(addr, 2);
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

#define port_CARDIsValidBlockNo(card, iBlock) \
    (PORT_CARD_DIR_NUM_SYSTEM_BLOCK <= (iBlock) && (iBlock) < (card)->cBlock)

#define TRUNC(n, a) (((uint32_t)(n)) & ~((a)-1))

/* ────────────────────────────────────────────────────────────────────
 * __CARDCompareFileName  (CARDOpen.c:8-31)
 *
 * Byte-by-byte comparison of dir entry fileName with search string.
 * fileName is u8[32] at offset 8 in CARDDir — endian-agnostic.
 * ──────────────────────────────────────────────────────────────────── */
int port_CARDCompareFileName(uint32_t dir_entry_addr, const char *fileName)
{
    uint32_t name_addr = dir_entry_addr + PORT_CARD_DIR_OFF_FILENAME;
    char c1, c2;
    int n;

    n = PORT_CARD_FILENAME_MAX;
    while (0 <= --n) {
        c1 = (char)load_u8(name_addr++);
        c2 = *fileName++;
        if (c1 != c2) {
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
 * __CARDAccess  (CARDOpen.c:33-45)
 *
 * Checks if the card's diskID matches the dir entry's gameName/company.
 * If diskID == &__CARDDiskNone, any file is accessible.
 * Byte fields — endian-agnostic.
 * ──────────────────────────────────────────────────────────────────── */
int32_t port_CARDAccess(port_CARDDirControl *card, uint32_t dir_entry_addr)
{
    uint8_t *p;

    if (load_u8(dir_entry_addr + PORT_CARD_DIR_OFF_GAMENAME) == 0xFF) {
        return PORT_CARD_DIR_RESULT_NOFILE;
    }

    if (card->diskID_is_none) {
        return PORT_CARD_DIR_RESULT_READY;
    }

    p = gc_mem_ptr(dir_entry_addr + PORT_CARD_DIR_OFF_GAMENAME, 4);
    if (memcmp(p, card->gameName, 4) == 0) {
        p = gc_mem_ptr(dir_entry_addr + PORT_CARD_DIR_OFF_COMPANY, 2);
        if (memcmp(p, card->company, 2) == 0) {
            return PORT_CARD_DIR_RESULT_READY;
        }
    }

    return PORT_CARD_DIR_RESULT_NOPERM;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDIsPublic  (CARDOpen.c:47-58)
 * ──────────────────────────────────────────────────────────────────── */
int32_t port_CARDIsPublic(uint32_t dir_entry_addr)
{
    if (load_u8(dir_entry_addr + PORT_CARD_DIR_OFF_GAMENAME) == 0xFF) {
        return PORT_CARD_DIR_RESULT_NOFILE;
    }

    if ((load_u8(dir_entry_addr + PORT_CARD_DIR_OFF_PERMISSION) & PORT_CARD_ATTR_PUBLIC) != 0) {
        return PORT_CARD_DIR_RESULT_READY;
    }

    return PORT_CARD_DIR_RESULT_NOPERM;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDGetFileNo  (CARDOpen.c:60-85)
 *
 * Scans all 127 dir entries for a matching filename.
 * Uses __CARDAccess + __CARDCompareFileName internally.
 * ──────────────────────────────────────────────────────────────────── */
int32_t port_CARDGetFileNo(port_CARDDirControl *card, const char *fileName,
                            int32_t *pfileNo)
{
    int32_t fileNo;
    int32_t result;

    if (!card->attached) {
        return PORT_CARD_DIR_RESULT_NOCARD;
    }

    for (fileNo = 0; fileNo < PORT_CARD_MAX_FILE; fileNo++) {
        uint32_t ent_addr = card->dir_addr + (uint32_t)fileNo * PORT_CARD_DIR_SIZE;
        result = port_CARDAccess(card, ent_addr);
        if (result < 0) {
            continue;
        }
        if (port_CARDCompareFileName(ent_addr, fileName)) {
            *pfileNo = fileNo;
            return PORT_CARD_DIR_RESULT_READY;
        }
    }

    return PORT_CARD_DIR_RESULT_NOFILE;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDSeek  (CARDRead.c:8-53)
 *
 * Walks the FAT chain from the file's current position to the target
 * offset. Pure computation over dir entry (startBlock, length) and
 * FAT (u16 chain). Both in gc_mem as big-endian.
 *
 * Hardware mocking:
 *   __CARDGetControlBlock → card passed directly, check attached
 *   __CARDPutControlBlock → return result code directly
 *   __CARDGetDirBlock     → card->dir_addr
 *   __CARDGetFatBlock     → card->fat_addr
 * ──────────────────────────────────────────────────────────────────── */
int32_t port_CARDSeek(port_CARDFileInfo *fileInfo, int32_t length, int32_t offset,
                       port_CARDDirControl *card)
{
    uint32_t ent_addr;
    uint16_t ent_length;
    uint16_t ent_startBlock;

    /* __CARDGetControlBlock mock */
    if (!card->attached) {
        return PORT_CARD_DIR_RESULT_NOCARD;
    }

    if (!port_CARDIsValidBlockNo(card, fileInfo->iBlock) ||
        card->cBlock * card->sectorSize <= fileInfo->offset) {
        return PORT_CARD_DIR_RESULT_FATAL_ERROR;
    }

    ent_addr = card->dir_addr + (uint32_t)fileInfo->fileNo * PORT_CARD_DIR_SIZE;
    ent_length = load_u16be(ent_addr + PORT_CARD_DIR_OFF_LENGTH);
    ent_startBlock = load_u16be(ent_addr + PORT_CARD_DIR_OFF_STARTBLOCK);

    if (ent_length * card->sectorSize <= offset ||
        ent_length * card->sectorSize < offset + length) {
        return PORT_CARD_DIR_RESULT_LIMIT;
    }

    /* card->fileInfo = fileInfo; -- side effect, not relevant for parity */
    fileInfo->length = length;
    if (offset < fileInfo->offset) {
        fileInfo->offset = 0;
        fileInfo->iBlock = ent_startBlock;
        if (!port_CARDIsValidBlockNo(card, fileInfo->iBlock)) {
            return PORT_CARD_DIR_RESULT_BROKEN;
        }
    }

    /* Walk FAT chain */
    while (fileInfo->offset < (int32_t)TRUNC(offset, card->sectorSize)) {
        fileInfo->offset += card->sectorSize;
        fileInfo->iBlock = load_u16be(card->fat_addr + (uint32_t)fileInfo->iBlock * 2);
        if (!port_CARDIsValidBlockNo(card, fileInfo->iBlock)) {
            return PORT_CARD_DIR_RESULT_BROKEN;
        }
    }

    fileInfo->offset = offset;
    return PORT_CARD_DIR_RESULT_READY;
}
