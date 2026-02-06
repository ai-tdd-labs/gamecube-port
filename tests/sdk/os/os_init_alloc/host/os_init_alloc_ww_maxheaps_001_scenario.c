#include <stdint.h>
#include <stddef.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void *OSInitAlloc(void *arenaStart, void *arenaEnd, int maxHeaps);

extern volatile int32_t __OSCurrHeap;
extern uint32_t __gc_osalloc_heap_array;
extern int32_t __gc_osalloc_num_heaps;
extern uint32_t __gc_osalloc_arena_start;
extern uint32_t __gc_osalloc_arena_end;

const char *gc_scenario_label(void) {
    return "OSInitAlloc/os_init_alloc_ww_maxheaps_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/os_init_alloc_ww_maxheaps_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    void *arena_lo = (void *)0x80004000u;
    void *arena_hi = (void *)0x81700000u;
    int max_heaps = 4;

    void *result = OSInitAlloc(arena_lo, arena_hi, max_heaps);

    // Read back the first HeapDesc from emulated RAM. OSInitAlloc should have
    // written it at arena_lo.
    uint8_t *hd0 = gc_ram_ptr(ram, __gc_osalloc_heap_array, 12);
    if (!hd0) die("gc_ram_ptr(hd0) failed");

    uint32_t hd0_size = rd32be(hd0 + 0);
    uint32_t hd0_free = rd32be(hd0 + 4);
    uint32_t hd0_alloc = rd32be(hd0 + 8);

    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!out) die("gc_ram_ptr(out) failed");

    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, (uint32_t)(uintptr_t)result);
    wr32be(out + 0x08, __gc_osalloc_heap_array);
    wr32be(out + 0x0C, (uint32_t)__gc_osalloc_num_heaps);

    wr32be(out + 0x10, __gc_osalloc_arena_start);
    wr32be(out + 0x14, __gc_osalloc_arena_end);

    wr32be(out + 0x18, hd0_size);
    wr32be(out + 0x1C, hd0_free);
    wr32be(out + 0x20, hd0_alloc);

    wr32be(out + 0x24, (uint32_t)__OSCurrHeap);
}
