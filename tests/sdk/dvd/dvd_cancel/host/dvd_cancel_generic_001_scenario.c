#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef int32_t  s32;

typedef struct {
    volatile s32 state;
} DVDCommandBlock;

s32 DVDCancel(DVDCommandBlock *block);

const char *gc_scenario_label(void) { return "DVDCancel/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/dvd_cancel_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    // Test 1: Cancel NULL block (returns -1).
    s32 r1 = DVDCancel((DVDCommandBlock *)0);

    // Test 2: Cancel idle block (state=0, returns 0).
    DVDCommandBlock blk2;
    blk2.state = 0;
    s32 r2 = DVDCancel(&blk2);

    // Test 3: Cancel already-canceled block (state=10, returns 0).
    DVDCommandBlock blk3;
    blk3.state = 10;
    s32 r3 = DVDCancel(&blk3);

    // Test 4: Cancel finished block (state=-1, returns 0).
    DVDCommandBlock blk4;
    blk4.state = -1;
    s32 r4 = DVDCancel(&blk4);

    // Test 5: Cancel busy block (state=1, transitions to 10, returns 0).
    DVDCommandBlock blk5;
    blk5.state = 1;
    s32 r5 = DVDCancel(&blk5);
    s32 s5 = blk5.state;

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x20);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x20; i += 4) wr32be(p + i, 0);

    u32 off = 0;
    wr32be(p + off, 0xDEADBEEFu); off += 4;
    wr32be(p + off, (u32)r1); off += 4;
    wr32be(p + off, (u32)r2); off += 4;
    wr32be(p + off, (u32)blk2.state); off += 4;
    wr32be(p + off, (u32)r3); off += 4;
    wr32be(p + off, (u32)r4); off += 4;
    wr32be(p + off, (u32)r5); off += 4;
    wr32be(p + off, (u32)s5); off += 4;
}
