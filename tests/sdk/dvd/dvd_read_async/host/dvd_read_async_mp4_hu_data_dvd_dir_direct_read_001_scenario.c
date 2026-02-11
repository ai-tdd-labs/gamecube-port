#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef int32_t s32;

typedef struct { volatile s32 state; } DVDCommandBlock;
typedef struct {
  DVDCommandBlock cb;
  u32 startAddr;
  u32 length;
  s32 entrynum;
} DVDFileInfo;

typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

// sdk_port test hooks
void gc_dvd_test_reset_files(void);
void gc_dvd_test_set_file(s32 entrynum, const void *data, u32 len);

int DVDGetCommandBlockStatus(DVDCommandBlock *block);
int DVDFastOpen(s32 entrynum, DVDFileInfo *file);
s32 DVDReadAsync(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb);

extern u32 gc_dvd_async_busy_seen;

const char *gc_scenario_label(void) { return "DVDReadAsync/mp4_hu_data_dvd_dir_direct_read"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_read_async_mp4_hu_data_dvd_dir_direct_read_001.bin"; }

void gc_scenario_run(GcRam *ram) {
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

  gc_dvd_test_reset_files();
  gc_dvd_test_set_file(0, k_file, (u32)sizeof(k_file));

  DVDFileInfo fi;
  int ok = DVDFastOpen(0, &fi);

  uint8_t *dest = gc_ram_ptr(ram, 0x80400000u, 0x40);
  if (!dest) die("gc_ram_ptr dest failed");
  memset(dest, 0xAA, 0x40);

  gc_dvd_async_busy_seen = 0;
  s32 r = DVDReadAsync(&fi, dest, 0x20, 0x10, NULL);
  int st = DVDGetCommandBlockStatus(&fi.cb);

  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
  if (!p) die("gc_ram_ptr out failed");
  memset(p, 0, 0x40);
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, (u32)ok);
  wr32be(p + 0x08, (u32)r);
  wr32be(p + 0x0C, (u32)st);
  wr32be(p + 0x10, rd32be(dest + 0x00));
  wr32be(p + 0x14, rd32be(dest + 0x10));
  wr32be(p + 0x18, gc_dvd_async_busy_seen);
}
