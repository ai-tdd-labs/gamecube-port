#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef int32_t s32;

typedef struct { volatile s32 state; } DVDCommandBlock;
typedef struct {
    DVDCommandBlock cb;
    uint32_t startAddr;
    uint32_t length;
    s32 entrynum;
} DVDFileInfo;

// sdk_port test hooks
void gc_dvd_test_reset_paths(void);
void gc_dvd_test_set_paths(const char **paths, int32_t count);
void gc_dvd_test_reset_files(void);
void gc_dvd_test_set_file(int32_t entrynum, const void *data, uint32_t len);

extern uint32_t gc_dvd_read_calls;
extern uint32_t gc_dvd_last_read_len;
extern uint32_t gc_dvd_last_read_off;

int DVDOpen(const char *path, DVDFileInfo *fi);
int DVDRead(DVDFileInfo *fi, void *addr, int len, int off);

const char *gc_scenario_label(void) { return "DVDRead/mp4_init_mem"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_read_mp4_init_mem_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    static const char *k_paths[] = { "/meminfo.bin" };
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

    gc_dvd_read_calls = 0;
    gc_dvd_last_read_len = 0;
    gc_dvd_last_read_off = 0;

    DVDFileInfo fi;
    gc_dvd_test_reset_paths();
    gc_dvd_test_set_paths(k_paths, 1);
    gc_dvd_test_reset_files();
    gc_dvd_test_set_file(0, k_file, (uint32_t)sizeof(k_file));

    int ok = DVDOpen("/meminfo.bin", &fi);

    uint8_t *dest = gc_ram_ptr(ram, 0x81230000u, 0x40);
    if (!dest) die("gc_ram_ptr dest failed");
    memset(dest, 0xAA, 0x40);

    int r = DVDRead(&fi, dest, 0x20, 0x10);

    // Include gc_dvd_last_read_len/off so mutation tests can detect
    // swapped arguments even if the copied bytes look correct.
    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x1Cu);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, (uint32_t)ok);
    wr32be(p + 0x08, (uint32_t)r);
    wr32be(p + 0x0C, fi.length);
    wr32be(p + 0x10, rd32be(dest + 0x00));
    wr32be(p + 0x14, gc_dvd_last_read_len);
    wr32be(p + 0x18, gc_dvd_last_read_off);
}
