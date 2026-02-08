#pragma once

#include "dolphin/types.h"

// Minimal DVD header surface for MP4 workload compilation.

typedef struct {
    volatile s32 state; // 0 = idle, 1 = busy
} DVDCommandBlock;

typedef struct {
    DVDCommandBlock cb;
    u32 startAddr;
    u32 length;
    s32 entrynum;
} DVDFileInfo;

void DVDInit(void);
int DVDOpen(const char *path, DVDFileInfo *file);
int DVDRead(DVDFileInfo *file, void *addr, int len, int offset);
int DVDClose(DVDFileInfo *file);

