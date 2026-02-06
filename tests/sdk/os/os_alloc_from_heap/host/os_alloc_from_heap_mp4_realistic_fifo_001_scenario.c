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
    return "OSAllocFromHeap/os_alloc_from_heap_mp4_realistic_fifo_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/os_alloc_from_heap_mp4_realistic_fifo_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    void *arena_lo = (void *)0x80004000u;
    void *arena_hi = (void *)0x81700000u;
    void *new_lo = OSInitAlloc(arena_lo, arena_hi, 1);
    int heap = OSCreateHeap(new_lo, arena_hi);
    OSSetCurrentHeap(heap);

    void *p = OSAllocFromHeap((int)__OSCurrHeap, 0x100000u);

    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!out) die("gc_ram_ptr(out) failed");

    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, (uint32_t)(uintptr_t)p);
    wr32be(out + 0x08, (uint32_t)heap);
    wr32be(out + 0x0C, (uint32_t)__OSCurrHeap);
}

