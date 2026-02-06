#include <stdint.h>
#include <stddef.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps);
int OSCreateHeap(void *start, void *end);

extern uint32_t __gc_osalloc_heap_array;

const char *gc_scenario_label(void) {
    return "OSCreateHeap/os_create_heap_ww_jkrstdheap_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/os_create_heap_ww_jkrstdheap_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    void *arena_lo = (void *)0x80004000u;
    void *arena_hi = (void *)0x81700000u;

    void *new_lo = OSInitAlloc(arena_lo, arena_hi, 1);

    void *heap_start = (void *)(0x80010000u);
    void *heap_end = (void *)0x80014000u;

    int heap = OSCreateHeap(heap_start, heap_end);

    // Read back hd0 from emulated RAM.
    uint8_t *hd0 = gc_ram_ptr(ram, __gc_osalloc_heap_array, 12);
    if (!hd0) die("gc_ram_ptr(hd0) failed");

    uint32_t hd0_size = rd32be(hd0 + 0);
    uint32_t hd0_free = rd32be(hd0 + 4);
    uint32_t hd0_alloc = rd32be(hd0 + 8);

    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!out) die("gc_ram_ptr(out) failed");

    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, (uint32_t)heap);
    wr32be(out + 0x08, hd0_size);
    wr32be(out + 0x0C, hd0_free);
    wr32be(out + 0x10, hd0_alloc);
}
