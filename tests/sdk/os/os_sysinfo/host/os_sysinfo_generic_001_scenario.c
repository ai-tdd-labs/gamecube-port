#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t  u8;

/* OSSystem.c */
u32 OSGetConsoleType(void);
u32 OSGetPhysicalMemSize(void);
u32 OSGetConsoleSimulatedMemSize(void);

/* OSRtc.c */
u32 OSGetProgressiveMode(void);
u32 OSGetTick(void);

extern u8 gc_sram_flags;
extern u32 gc_sram_unlock_calls;
extern u64 gc_os_tick_counter;

const char *gc_scenario_label(void) { return "OS_SYSINFO/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/os_sysinfo_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x60);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x60; i += 4) wr32be(p + i, 0);

    u32 off = 0;

    /* --- T1: System info getters --- */
    u32 ct = OSGetConsoleType();
    u32 pm = OSGetPhysicalMemSize();
    u32 sm = OSGetConsoleSimulatedMemSize();
    wr32be(p + off, 0x5150A001u); off += 4;
    wr32be(p + off, ct); off += 4;
    wr32be(p + off, pm); off += 4;
    wr32be(p + off, sm); off += 4;

    /* --- T2: OSGetProgressiveMode with flags=0 --- */
    gc_sram_flags = 0;
    gc_sram_unlock_calls = 0;
    u32 prog0 = OSGetProgressiveMode();
    wr32be(p + off, 0x5150A002u); off += 4;
    wr32be(p + off, prog0); off += 4;
    wr32be(p + off, gc_sram_unlock_calls); off += 4;

    /* --- T3: OSGetProgressiveMode with flags=0x80 --- */
    gc_sram_flags = 0x80;
    gc_sram_unlock_calls = 0;
    u32 prog1 = OSGetProgressiveMode();
    wr32be(p + off, 0x5150A003u); off += 4;
    wr32be(p + off, prog1); off += 4;
    wr32be(p + off, gc_sram_unlock_calls); off += 4;

    /* --- T4: OSGetProgressiveMode with flags=0xFF --- */
    gc_sram_flags = 0xFF;
    gc_sram_unlock_calls = 0;
    u32 prog2 = OSGetProgressiveMode();
    wr32be(p + off, 0x5150A004u); off += 4;
    wr32be(p + off, prog2); off += 4;
    wr32be(p + off, gc_sram_unlock_calls); off += 4;

    /* --- T5: OSGetTick (3 calls) --- */
    gc_os_tick_counter = 0;
    u32 t1 = OSGetTick();
    u32 t2 = OSGetTick();
    u32 t3 = OSGetTick();
    wr32be(p + off, 0x5150A005u); off += 4;
    wr32be(p + off, t1); off += 4;
    wr32be(p + off, t2); off += 4;
    wr32be(p + off, t3); off += 4;
}
