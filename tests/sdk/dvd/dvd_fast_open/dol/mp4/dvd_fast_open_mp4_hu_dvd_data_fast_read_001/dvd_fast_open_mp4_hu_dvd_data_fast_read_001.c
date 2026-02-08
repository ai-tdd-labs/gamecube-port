#include <stdint.h>

typedef uint32_t u32;
typedef int32_t s32;

// MP4 callsite reference:
//   decomp_mario_party_4/src/game/dvd.c: HuDvdDataFastRead()
//   It calls DVDFastOpen(entrynum, &file) and then uses file.length.

typedef struct {
  volatile s32 state;
} DVDCommandBlock;

typedef struct {
  DVDCommandBlock cb;
  u32 startAddr;
  u32 length;
  s32 entrynum;
} DVDFileInfo;

// Deterministic oracle: we pretend entrynum 0 exists and has fixed length.
#ifndef GC_HOST_TEST
static int DVDFastOpen(s32 entrynum, DVDFileInfo* file) {
  if (!file) return 0;
  if (entrynum != 0) return 0;
  file->cb.state = 0;
  file->entrynum = entrynum;
  file->startAddr = (u32)entrynum;
  file->length = 0x40;
  return 1;
}
#else
int DVDFastOpen(s32 entrynum, DVDFileInfo* file);
#endif

static volatile u32 *OUT = (u32 *)0x80300000u;

int main(void) {
  DVDFileInfo fi;
  int ok = DVDFastOpen(0, &fi);

  OUT[0] = 0xDEADBEEFu;
  OUT[1] = (u32)ok;
  OUT[2] = fi.startAddr;
  OUT[3] = fi.length;
  OUT[4] = (u32)fi.entrynum;

  for (;;) {}
  return 0;
}

