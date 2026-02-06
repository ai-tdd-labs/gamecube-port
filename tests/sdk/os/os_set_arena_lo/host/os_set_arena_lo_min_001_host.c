#include <stdint.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_test.h"

// SDK port under test
void OSSetArenaLo(void *addr);
void *OSGetArenaLo(void);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    GcRam ram;
    if (gc_ram_init(&ram, 0x80000000u, 0x02000000u) != 0) { // 32 MiB
        die("gc_ram_init failed");
    }

    // Test logic (mirrors DOL case): store fixed pointer and read back.
    OSSetArenaLo((void*)0x81234560u);

    uint8_t *p0 = gc_ram_ptr(&ram, 0x80300000u, 8);
    if (!p0) die("gc_ram_ptr failed");

    wr32be(p0 + 0, 0xDEADBEEFu);
    wr32be(p0 + 4, (uint32_t)(uintptr_t)OSGetArenaLo());

    if (gc_ram_dump(&ram, 0x80300000u, 0x40, "../actual/os_set_arena_lo_min_001.bin") != 0) {
        die("dump failed");
    }

    gc_ram_free(&ram);
    return 0;
}
