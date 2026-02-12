/*
 * sdk_port/card/card_dir.h --- Host-side port of GC SDK CARD directory ops.
 *
 * CARDDir entries live in gc_mem (big-endian layout, 64 bytes each).
 * Byte fields (gameName, company, fileName, permission) are endian-agnostic.
 * Multi-byte fields (startBlock, length, etc.) use BE load/store.
 *
 * Source of truth:
 *   external/mp4-decomp/src/dolphin/card/CARDOpen.c
 *   external/mp4-decomp/src/dolphin/card/CARDRead.c
 *   external/mp4-decomp/include/dolphin/CARDPriv.h
 */
#pragma once

#include <stdint.h>

/* CARDDir field offsets (64-byte struct in gc_mem) */
#define PORT_CARD_DIR_OFF_GAMENAME    0   /* u8[4] */
#define PORT_CARD_DIR_OFF_COMPANY     4   /* u8[2] */
#define PORT_CARD_DIR_OFF_FILENAME    8   /* u8[32] */
#define PORT_CARD_DIR_OFF_PERMISSION  52  /* u8 */
#define PORT_CARD_DIR_OFF_STARTBLOCK  54  /* u16 BE */
#define PORT_CARD_DIR_OFF_LENGTH      56  /* u16 BE */
#define PORT_CARD_DIR_SIZE            64

/* Constants (from card.h / CARDPriv.h) */
#define PORT_CARD_FILENAME_MAX     32
#define PORT_CARD_MAX_FILE         127
#define PORT_CARD_DIR_NUM_SYSTEM_BLOCK 5
#define PORT_CARD_ATTR_PUBLIC      0x04u

/* Result codes */
#define PORT_CARD_DIR_RESULT_READY       0
#define PORT_CARD_DIR_RESULT_NOCARD     -3
#define PORT_CARD_DIR_RESULT_NOFILE     -4
#define PORT_CARD_DIR_RESULT_BROKEN     -6
#define PORT_CARD_DIR_RESULT_NOPERM    -10
#define PORT_CARD_DIR_RESULT_LIMIT     -11
#define PORT_CARD_DIR_RESULT_FATAL_ERROR -128

/* Control struct for directory + seek operations */
typedef struct {
    int      attached;
    uint16_t cBlock;        /* total blocks (5 system + data) */
    int32_t  sectorSize;    /* bytes per sector (typically 8192) */
    uint32_t dir_addr;      /* gc_mem address of dir block (127 entries) */
    uint32_t fat_addr;      /* gc_mem address of FAT (u16 array) */
    uint8_t  gameName[4];   /* diskID.gameName */
    uint8_t  company[2];    /* diskID.company */
    int      diskID_is_none;/* if set, __CARDAccess passes for any game */
} port_CARDDirControl;

/* CARDFileInfo (host-side, mirrors decomp layout) */
typedef struct {
    int32_t  chan;
    int32_t  fileNo;
    int32_t  offset;
    int32_t  length;
    uint16_t iBlock;
} port_CARDFileInfo;

int     port_CARDCompareFileName(uint32_t dir_entry_addr, const char *fileName);
int32_t port_CARDAccess(port_CARDDirControl *card, uint32_t dir_entry_addr);
int32_t port_CARDIsPublic(uint32_t dir_entry_addr);
int32_t port_CARDGetFileNo(port_CARDDirControl *card, const char *fileName,
                            int32_t *pfileNo);
int32_t port_CARDSeek(port_CARDFileInfo *fileInfo, int32_t length, int32_t offset,
                       port_CARDDirControl *card);
