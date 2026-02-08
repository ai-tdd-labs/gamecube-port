#include "gc_host_ram.h"
#include "gc_host_scenario.h"
#include "gc_host_test.h"

#include "gc_mem.h"

#include "sdk_state.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    GcRam ram;
    if (gc_ram_init(&ram, 0x80000000u, 0x02000000u) != 0) { // 32 MiB
        die("gc_ram_init failed");
    }

    // Make sdk_port address translation available for the scenario.
    gc_mem_set(ram.base, ram.size, ram.buf);
    gc_sdk_state_reset();

    gc_scenario_run(&ram);

    // Default dump region matches the Dolphin dumps in tools/run_tests.sh.
    const uint32_t dump_addr = 0x80300000u;
    const size_t dump_size = 0x40;

    const char *out_path = gc_scenario_out_path();
    if (!out_path) die("gc_scenario_out_path returned NULL");

    if (gc_ram_dump(&ram, dump_addr, dump_size, out_path) != 0) {
        die("gc_ram_dump failed");
    }

    // Optional extra dump (e.g. MEM1) for checkpoint comparisons.
    //
    // Environment variables:
    // - GC_HOST_DUMP_ADDR: hex/dec address (e.g. 0x80000000)
    // - GC_HOST_DUMP_SIZE: hex/dec size (e.g. 0x01800000)
    // - GC_HOST_DUMP_PATH: output path (optional; defaults to "<out_path>.mem1.bin")
    const char *env_addr = getenv("GC_HOST_DUMP_ADDR");
    const char *env_size = getenv("GC_HOST_DUMP_SIZE");
    if (env_addr && env_size) {
        char *endp = 0;
        uint32_t big_addr = (uint32_t)strtoul(env_addr, &endp, 0);
        if (!endp || *endp != 0) die("invalid GC_HOST_DUMP_ADDR");

        endp = 0;
        size_t big_size = (size_t)strtoull(env_size, &endp, 0);
        if (!endp || *endp != 0) die("invalid GC_HOST_DUMP_SIZE");

        const char *big_path = getenv("GC_HOST_DUMP_PATH");
        char derived_path[4096];
        if (!big_path || !*big_path) {
            if (snprintf(derived_path, sizeof(derived_path), "%s.mem1.bin", out_path) >=
                (int)sizeof(derived_path)) {
                die("derived dump path too long");
            }
            big_path = derived_path;
        }

        if (gc_ram_dump(&ram, big_addr, big_size, big_path) != 0) {
            die("gc_ram_dump big failed");
        }
    }

    gc_ram_free(&ram);
    return 0;
}
