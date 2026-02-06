#ifndef _DOLPHIN_OSBOOTINFO
#define _DOLPHIN_OSBOOTINFO

#include "dolphin/types.h"
#include "dolphin/dvd.h"

typedef struct OSBootInfo_s {
  DVDDiskID DVDDiskID;
  u32 magic;
  u32 version;
  u32 memorySize;
  u32 consoleType;
  void* arenaLo;
  void* arenaHi;
  void* FSTLocation;
  u32 FSTMaxLength;
} OSBootInfo;

#define OS_BOOTINFO_MAGIC 0x0D15EA5E
#define OS_BOOTINFO_MAGIC_JTAG 0xE5207C22

#endif // _DOLPHIN_OSBOOTINFO
