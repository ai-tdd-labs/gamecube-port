#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void OSInitAlloc(void *lo, void *hi, int max_heaps);
int OSCreateHeap(void *lo, void *hi);
void OSSetCurrentHeap(int heap);
void *OSAlloc(uint32_t size);
extern volatile int32_t __OSCurrHeap;

const char *gc_scenario_label(void) { return "OSAlloc/mp4_hu_sys_init"; }
const char *gc_scenario_out_path(void) { return "../actual/os_alloc_mp4_default_fifo_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    // Match the MP4-style testcase memory layout (large enough for 0x100000 alloc).
    OSInitAlloc((void*)0x80002000u, (void*)0x80200000u, 2);
    int heap = OSCreateHeap((void*)0x80003000u, (void*)0x80200000u);
    OSSetCurrentHeap(heap);

    void *p1 = OSAlloc(0x100000);
    void *p2 = (void*)0;// unused


    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 12);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, (uint32_t)(uintptr_t)p1);
    wr32be(p + 8, (uint32_t)(uintptr_t)p2);
}
