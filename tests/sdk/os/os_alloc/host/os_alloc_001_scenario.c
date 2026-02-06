#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void OSInitAlloc(void *lo, void *hi, int max_heaps);
int OSCreateHeap(void *lo, void *hi);
void OSSetCurrentHeap(int heap);
void *OSAllocFromHeap(int heap, uint32_t size);
extern volatile int32_t __OSCurrHeap;

const char *gc_scenario_label(void) { return "OSAlloc/legacy_001"; }
const char *gc_scenario_out_path(void) { return "../actual/os_alloc_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    OSInitAlloc((void*)0x80002000u, (void*)0x80008000u, 2);
    int heap = OSCreateHeap((void*)0x80003000u, (void*)0x80007000u);
    OSSetCurrentHeap(heap);

    void *p1 = OSAllocFromHeap(__OSCurrHeap, 0x20);
    void *p2 = OSAllocFromHeap(__OSCurrHeap, 0x40);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 12);
    if (!p) die("gc_ram_ptr failed");
    wr32be(p + 0, 0xDEADBEEFu);
    wr32be(p + 4, (uint32_t)(uintptr_t)p1);
    wr32be(p + 8, (uint32_t)(uintptr_t)p2);
}
