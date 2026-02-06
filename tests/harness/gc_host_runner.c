#include "gc_host_ram.h"
#include "gc_host_scenario.h"
#include "gc_host_test.h"

#include <stdint.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    GcRam ram;
    if (gc_ram_init(&ram, 0x80000000u, 0x02000000u) != 0) { // 32 MiB
        die("gc_ram_init failed");
    }

    gc_scenario_run(&ram);

    // Default dump region matches the Dolphin dumps in tools/run_tests.sh.
    const uint32_t dump_addr = 0x80300000u;
    const size_t dump_size = 0x40;

    const char *out_path = gc_scenario_out_path();
    if (!out_path) die("gc_scenario_out_path returned NULL");

    if (gc_ram_dump(&ram, dump_addr, dump_size, out_path) != 0) {
        die("gc_ram_dump failed");
    }

    gc_ram_free(&ram);
    return 0;
}

