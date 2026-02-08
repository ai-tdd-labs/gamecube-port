#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef int32_t s32;

// MP4 callsite reference:
//   decomp_mario_party_4/src/game/dvd.c: HuDvdDataReadWait()
//   DVDReadAsync(file, buf, OSRoundUp32B(len), 0, cb);
//   callback sets a global (CallBackStatus=1) and the caller spin-waits.

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

static volatile u32 s_cb_called;
static volatile u32 s_cb_result;
static volatile u32 s_cb_start;

static void cb(s32 result, DVDFileInfo *fileInfo) {
  s_cb_called = 1;
  s_cb_result = (u32)result;
  s_cb_start = fileInfo ? fileInfo->startAddr : 0;
}

#ifndef GC_HOST_TEST
static int DVDFastOpen(s32 entrynum, DVDFileInfo* file) {
  if (!file) return 0;
  if (entrynum != 0) return 0;
  file->cb.state = 0;
  file->entrynum = entrynum;
  file->startAddr = (u32)entrynum;
  file->length = (u32)sizeof(k_file);
  return 1;
}

static s32 DVDReadAsync(DVDFileInfo* file, void* addr, s32 len, s32 offset, DVDCallback cbfn) {
  if (!file || !addr) return 0;
  if (offset < 0 || len < 0) return 0;
  u32 off = (u32)offset;
  u32 n = (u32)len;
  if (off > (u32)sizeof(k_file)) return 0;
  if (off + n > (u32)sizeof(k_file)) n = (u32)sizeof(k_file) - off;

  file->cb.state = 1;
  memcpy(addr, k_file + off, n);
  file->cb.state = 0;
  if (cbfn) cbfn((s32)n, file);
  return 1;
}
#else
int DVDFastOpen(s32 entrynum, DVDFileInfo* file);
s32 DVDReadAsync(DVDFileInfo* file, void* addr, s32 len, s32 offset, DVDCallback cbfn);
#endif

static volatile u32 *OUT = (u32 *)0x80300000u;

int main(void) {
  DVDFileInfo fi;
  int ok = DVDFastOpen(0, &fi);

  uint8_t *dest = (uint8_t *)0x80400000u;
  for (u32 i = 0; i < 0x40; i++) dest[i] = 0xAA;

  s_cb_called = 0;
  s_cb_result = 0;
  s_cb_start = 0;

  s32 r = DVDReadAsync(&fi, dest, 0x20, 0x00, cb);

  OUT[0] = 0xDEADBEEFu;
  OUT[1] = (u32)ok;
  OUT[2] = (u32)r;
  OUT[3] = (u32)s_cb_called;
  OUT[4] = (u32)s_cb_result;
  OUT[5] = (u32)s_cb_start;
  OUT[6] = *(volatile u32 *)(0x80400000u);

  for (;;) {}
  return 0;
}

