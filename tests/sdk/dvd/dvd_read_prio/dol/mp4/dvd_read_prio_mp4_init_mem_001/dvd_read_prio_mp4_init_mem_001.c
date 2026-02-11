#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef int32_t s32;

// MP4 callsite reference:
//   decomp_mario_party_4/src/game/init.c: LoadMemInfo()
//   if (DVDOpen("/meminfo.bin", &file)) { size=file.length; ... DVDReadPrio(&file, buf_ptr, ..., 2); }

typedef struct {
  u32 startAddr;
  u32 length;
  s32 entrynum;
} DVDFileInfo;

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

static int DVDOpen(const char *path, DVDFileInfo *fi) {
  if (!fi) return 0;
  if (!path || strcmp(path, "/meminfo.bin") != 0) return 0;
  fi->startAddr = 0;
  fi->entrynum = 0;
  fi->length = (u32)sizeof(k_file);
  return 1;
}

static int DVDRead(DVDFileInfo *fi, void *addr, int len, int off) {
  if (!fi || !addr) return -1;
  if (len < 0 || off < 0) return -1;
  u32 o = (u32)off;
  u32 n = (u32)len;
  if (o > (u32)sizeof(k_file)) return -1;
  if (o + n > (u32)sizeof(k_file)) n = (u32)sizeof(k_file) - o;
  memcpy(addr, k_file + o, n);
  return (int)n;
}

static int DVDReadPrio(DVDFileInfo *fi, void *addr, int len, int off, int prio) {
  (void)prio;
  return DVDRead(fi, addr, len, off) < 0 ? -1 : 0;
}

int main(void) {
    DVDFileInfo fi;
    int ok = DVDOpen("/meminfo.bin", &fi);
    int len = 0x20;
    int off = 0x10;
    int r = DVDReadPrio(&fi, (void*)0x81230000u, len, off, 2);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)ok;
    *(volatile u32*)0x80300008 = (u32)r;
    *(volatile u32*)0x8030000C = fi.length;
    *(volatile u32*)0x80300010 = *(volatile u32*)0x81230000u;
    // Extra observable state so mutation tests can kill len/off swap bugs.
    *(volatile u32*)0x80300014 = (u32)len;
    *(volatile u32*)0x80300018 = (u32)off;
    while (1) {}
}
