/*
 * card_fat.h --- Host-side port of GC SDK CARD FAT block allocator.
 *
 * FAT lives in gc_mem (big-endian), accessed via load/store helpers.
 */
#pragma once

#include <stdint.h>

/* FAT index constants */
#define PORT_CARD_FAT_AVAIL       0x0000u
#define PORT_CARD_FAT_CHECKSUM    0
#define PORT_CARD_FAT_CHECKSUMINV 1
#define PORT_CARD_FAT_CHECKCODE   2
#define PORT_CARD_FAT_FREEBLOCKS  3
#define PORT_CARD_FAT_LASTSLOT    4
#define PORT_CARD_NUM_SYSTEM_BLOCK 5

#define PORT_CARD_FAT_ENTRIES     4096
#define PORT_CARD_FAT_BYTES       (PORT_CARD_FAT_ENTRIES * 2)

/* Result codes */
#define PORT_CARD_RESULT_READY     0
#define PORT_CARD_RESULT_NOCARD   -3
#define PORT_CARD_RESULT_BROKEN   -6
#define PORT_CARD_RESULT_INSSPACE -9

/* Minimal CARDControl for FAT operations */
typedef struct {
    int      attached;
    uint16_t cBlock;      /* total blocks (5 system + data) */
    uint16_t startBlock;  /* output: first block of allocated chain */
    uint32_t fat_addr;    /* GC address of FAT array in gc_mem */
} port_CARDControl;

void    port_CARDCheckSum(uint32_t addr, int length,
                          uint16_t *checksum, uint16_t *checksumInv);
int32_t port_CARDUpdateFatBlock(port_CARDControl *card);
int32_t port_CARDAllocBlock(port_CARDControl *card, uint32_t cBlock);
int32_t port_CARDFreeBlock(port_CARDControl *card, uint16_t nBlock);
