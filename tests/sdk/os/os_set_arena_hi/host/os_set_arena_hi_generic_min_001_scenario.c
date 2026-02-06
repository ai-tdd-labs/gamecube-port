#include <stdint.h>
#include <stddef.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

void OSSetArenaHi(void *addr);
void *OSGetArenaHi(void);

const char *gc_scenario_label(void) {
    return "OSSetArenaHi/os_set_arena_hi_generic_min_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/os_set_arena_hi_generic_min_001.bin";
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    OSSetArenaHi((void *)0x81700000u);
    void *hi = OSGetArenaHi();

    uint8_t *out = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!out) die("gc_ram_ptr(out) failed");

    wr32be(out + 0x00, 0xDEADBEEFu);
    wr32be(out + 0x04, (uint32_t)(uintptr_t)hi);
}

