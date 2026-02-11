/*
 * sdk_port/card/card_fat.c --- Host-side port of GC SDK CARD FAT allocator.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/card/CARDBlock.c
 *                  external/mp4-decomp/src/dolphin/card/CARDCheck.c
 *
 * Same logic as decomp, but FAT lives in gc_mem (big-endian u16 access).
 * Hardware callbacks (DCStoreRange, EXI writes) are stripped.
 */
#include <stdint.h>
#include "card_fat.h"
#include "../gc_mem.h"

/* ── Big-endian u16 helpers ── */

static inline uint16_t load_u16be(uint32_t addr)
{
    uint8_t *p = gc_mem_ptr(addr, 2);
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline void store_u16be(uint32_t addr, uint16_t val)
{
    uint8_t *p = gc_mem_ptr(addr, 2);
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)(val);
}

/* Convenience: read/write fat[index] */
static inline uint16_t fat_get(port_CARDControl *card, uint16_t index)
{
    return load_u16be(card->fat_addr + (uint32_t)index * 2);
}

static inline void fat_set(port_CARDControl *card, uint16_t index, uint16_t val)
{
    store_u16be(card->fat_addr + (uint32_t)index * 2, val);
}

#define port_CARDIsValidBlockNo(card, iBlock) \
    (PORT_CARD_NUM_SYSTEM_BLOCK <= (iBlock) && (iBlock) < (card)->cBlock)

/* ────────────────────────────────────────────────────────────────────
 * __CARDCheckSum  (CARDCheck.c:11-28)
 *
 * Reads u16 values from gc_mem in big-endian.
 * checksum/checksumInv are host-side outputs (not stored in gc_mem here;
 * the caller writes them back via store_u16be).
 * ──────────────────────────────────────────────────────────────────── */
void port_CARDCheckSum(uint32_t addr, int length,
                       uint16_t *checksum, uint16_t *checksumInv)
{
    int i;
    length /= sizeof(uint16_t);
    *checksum = *checksumInv = 0;
    for (i = 0; i < length; i++) {
        uint16_t val = load_u16be(addr + (uint32_t)i * 2);
        *checksum += val;
        *checksumInv += ~val;
    }
    if (*checksum == 0xffff) {
        *checksum = 0;
    }
    if (*checksumInv == 0xffff) {
        *checksumInv = 0;
    }
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDUpdateFatBlock  (CARDBlock.c:159-170, hardware stripped)
 * ──────────────────────────────────────────────────────────────────── */
int32_t port_CARDUpdateFatBlock(port_CARDControl *card)
{
    uint32_t fat = card->fat_addr;

    /* ++fat[CHECKCODE] */
    uint16_t counter = load_u16be(fat + PORT_CARD_FAT_CHECKCODE * 2);
    counter++;
    store_u16be(fat + PORT_CARD_FAT_CHECKCODE * 2, counter);

    /* __CARDCheckSum(&fat[CHECKCODE], 0x1FFC, &fat[CHECKSUM], &fat[CHECKSUMINV]) */
    uint16_t checksum, checksumInv;
    port_CARDCheckSum(fat + PORT_CARD_FAT_CHECKCODE * 2, 0x1FFC,
                      &checksum, &checksumInv);
    store_u16be(fat + PORT_CARD_FAT_CHECKSUM * 2, checksum);
    store_u16be(fat + PORT_CARD_FAT_CHECKSUMINV * 2, checksumInv);

    return PORT_CARD_RESULT_READY;
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDAllocBlock  (CARDBlock.c:82-131)
 * ──────────────────────────────────────────────────────────────────── */
int32_t port_CARDAllocBlock(port_CARDControl *card, uint32_t cBlock)
{
    uint16_t iBlock;
    uint16_t startBlock;
    uint16_t prevBlock = 0;
    uint16_t count;
    uint16_t freeBlocks;

    if (!card->attached) {
        return PORT_CARD_RESULT_NOCARD;
    }

    freeBlocks = fat_get(card, PORT_CARD_FAT_FREEBLOCKS);
    if (freeBlocks < cBlock) {
        return PORT_CARD_RESULT_INSSPACE;
    }

    fat_set(card, PORT_CARD_FAT_FREEBLOCKS, (uint16_t)(freeBlocks - cBlock));
    startBlock = 0xFFFF;
    iBlock = fat_get(card, PORT_CARD_FAT_LASTSLOT);
    count = 0;
    while (0 < cBlock) {
        if (card->cBlock - PORT_CARD_NUM_SYSTEM_BLOCK < ++count) {
            return PORT_CARD_RESULT_BROKEN;
        }

        iBlock++;
        if (!port_CARDIsValidBlockNo(card, iBlock)) {
            iBlock = PORT_CARD_NUM_SYSTEM_BLOCK;
        }

        if (fat_get(card, iBlock) == PORT_CARD_FAT_AVAIL) {
            if (startBlock == 0xFFFF) {
                startBlock = iBlock;
            }
            else {
                fat_set(card, prevBlock, iBlock);
            }
            prevBlock = iBlock;
            fat_set(card, iBlock, 0xFFFF);
            --cBlock;
        }
    }
    fat_set(card, PORT_CARD_FAT_LASTSLOT, iBlock);
    card->startBlock = startBlock;

    return port_CARDUpdateFatBlock(card);
}

/* ────────────────────────────────────────────────────────────────────
 * __CARDFreeBlock  (CARDBlock.c:133-157)
 * ──────────────────────────────────────────────────────────────────── */
int32_t port_CARDFreeBlock(port_CARDControl *card, uint16_t nBlock)
{
    uint16_t nextBlock;

    if (!card->attached) {
        return PORT_CARD_RESULT_NOCARD;
    }

    while (nBlock != 0xFFFF) {
        if (!port_CARDIsValidBlockNo(card, nBlock)) {
            return PORT_CARD_RESULT_BROKEN;
        }

        nextBlock = fat_get(card, nBlock);
        fat_set(card, nBlock, 0);
        nBlock = nextBlock;
        fat_set(card, PORT_CARD_FAT_FREEBLOCKS,
                (uint16_t)(fat_get(card, PORT_CARD_FAT_FREEBLOCKS) + 1));
    }

    return port_CARDUpdateFatBlock(card);
}
