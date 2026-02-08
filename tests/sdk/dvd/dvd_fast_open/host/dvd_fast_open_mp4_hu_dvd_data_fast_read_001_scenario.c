#include <stdint.h>

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

// sdk_port test hooks
void gc_dvd_test_reset_files(void);
void gc_dvd_test_set_file(s32 entrynum, const void *data, u32 len);

int DVDFastOpen(s32 entrynum, DVDFileInfo *file);

const char *gc_scenario_label(void) { return "DVDFastOpen/mp4_hu_dvd_data_fast_read"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_fast_open_mp4_hu_dvd_data_fast_read_001.bin"; }

void gc_scenario_run(GcRam *ram) {
  static const uint8_t k_file[0x40] = {0};

  gc_dvd_test_reset_files();
  gc_dvd_test_set_file(0, k_file, (u32)sizeof(k_file));

  DVDFileInfo fi;
  int ok = DVDFastOpen(0, &fi);

  uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
  if (!p) die("gc_ram_ptr failed");
  wr32be(p + 0x00, 0xDEADBEEFu);
  wr32be(p + 0x04, (u32)ok);
  wr32be(p + 0x08, fi.startAddr);
  wr32be(p + 0x0C, fi.length);
  wr32be(p + 0x10, (u32)fi.entrynum);
}

