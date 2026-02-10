#include <stdint.h>
#include <string.h>

#include <ogc/cache.h>

typedef uint32_t u32;
typedef int32_t s32;

// MP4 callsite reference:
//   decomp_mario_party_4/src/game/data.c: HuDataDVDdirDirectRead()
//   It does:
//     result = DVDReadAsync(fileInfo, dest, len, offset, NULL);
//     while (DVDGetCommandBlockStatus(&fileInfo->cb)) { ... }

typedef struct { volatile s32 state; } DVDCommandBlock;
typedef struct {
  DVDCommandBlock cb;
  u32 startAddr;
  u32 length;
  s32 entrynum;
} DVDFileInfo;

typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

static const uint8_t k_file[0x40] = {
  0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07,
  0x08,0x09,0x0A,0x0B, 0x0C,0x0D,0x0E,0x0F,
  0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17,
  0x18,0x19,0x1A,0x1B, 0x1C,0x1D,0x1E,0x1F,
  0x20,0x21,0x22,0x23, 0x24,0x25,0x26,0x27,
  0x28,0x29,0x2A,0x2B, 0x2C,0x2D,0x2E,0x2F,
  0x30,0x31,0x32,0x33, 0x34,0x35,0x36,0x37,
  0x38,0x39,0x3A,0x3B, 0x3C,0x3D,0x3E,0x3F,
};

#ifndef GC_HOST_TEST
static u32 s_busy_seen;

static int DVDGetCommandBlockStatus(DVDCommandBlock *block) {
  if (!block) return 0;
  return block->state;
}

static int DVDFastOpen(s32 entrynum, DVDFileInfo* file) {
  if (!file) return 0;
  if (entrynum != 0) return 0;
  file->cb.state = 0;
  file->entrynum = entrynum;
  file->startAddr = (u32)entrynum;
  file->length = (u32)sizeof(k_file);
  return 1;
}

static s32 DVDReadAsync(DVDFileInfo* file, void* addr, s32 len, s32 offset, DVDCallback cb) {
  if (!file || !addr) return 0;
  if (offset < 0 || len < 0) return 0;
  u32 off = (u32)offset;
  u32 n = (u32)len;
  if (off > (u32)sizeof(k_file)) return 0;
  if (off + n > (u32)sizeof(k_file)) n = (u32)sizeof(k_file) - off;

  file->cb.state = 1;
  s_busy_seen = 1;
  memcpy(addr, k_file + off, n);
  file->cb.state = 0;
  if (cb) cb((s32)n, file);
  return 1;
}
#else
int DVDGetCommandBlockStatus(DVDCommandBlock *block);
int DVDFastOpen(s32 entrynum, DVDFileInfo* file);
s32 DVDReadAsync(DVDFileInfo* file, void* addr, s32 len, s32 offset, DVDCallback cb);
#endif

static volatile u32 *OUT = (u32 *)0x80300000u;

int main(void) {
  DVDFileInfo fi;
  int ok = DVDFastOpen(0, &fi);

  uint8_t *dest = (uint8_t *)0x80400000u;
  for (u32 i = 0; i < 0x40; i++) dest[i] = 0xAA;

  s_busy_seen = 0;
  s32 r = DVDReadAsync(&fi, dest, 0x20, 0x10, NULL);
  int st = DVDGetCommandBlockStatus(&fi.cb);

  for (u32 i = 0; i < 16; i++) OUT[i] = 0;
  OUT[0] = 0xDEADBEEFu;
  OUT[1] = (u32)ok;
  OUT[2] = (u32)r;
  OUT[3] = (u32)st;
  OUT[4] = *(volatile u32 *)(0x80400000u); // first 4 bytes of dest
  OUT[5] = *(volatile u32 *)(0x80400010u); // bytes 0x10..0x13 of dest
  OUT[6] = s_busy_seen;

  // Make the debugger's RAM reads deterministic.
  DCFlushRange((void *)0x80400000u, 0x40);
  DCFlushRange((void *)0x80300000u, 0x40);

  for (;;) {}
  return 0;
}
