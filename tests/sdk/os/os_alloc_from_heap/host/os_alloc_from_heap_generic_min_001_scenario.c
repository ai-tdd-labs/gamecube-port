#include <stdint.h>
#include <stddef.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps);
int OSCreateHeap(void *start, void *end);
int OSSetCurrentHeap(int heap);
void *OSAllocFromHeap(int heap, uint32_t size);
extern volatile int32_t __OSCurrHeap;

const char *gc_scenario_label(void) {
    return "OSAllocFromHeap/os_alloc_from_heap_generic_min_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/os_alloc_from_heap_generic_min_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    OSInitAlloc((void *)0x80002000u, (void *)0x80008000u, 2);
    int heap = OSCreateHeap((void *)0x80003000u, (void *)0x80007000u);
    OSSetCurrentHeap(heap);

    void *p1 = OSAllocFromHeap((int)__OSCurrHeap, 0x20u);
    void *p2 = OSAllocFromHeap((int)__OSCurrHeap, 0x40u);

    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!out) die("gc_ram_ptr(out) failed");

    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, (uint32_t)(uintptr_t)p1);
    wr32be(out + 0x08, (uint32_t)(uintptr_t)p2);
}

