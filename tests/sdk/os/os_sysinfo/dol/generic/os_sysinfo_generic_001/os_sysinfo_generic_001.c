#include <stdint.h>

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

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

int main(void) {
    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x60; i++) out[i] = 0;

    u32 off = 0;

    /* --- T1: System info getters --- */
    u32 ct = OSGetConsoleType();
    u32 pm = OSGetPhysicalMemSize();
    u32 sm = OSGetConsoleSimulatedMemSize();
    wr32be(out + off, 0x5150A001u); off += 4; /* tag */
    wr32be(out + off, ct); off += 4;
    wr32be(out + off, pm); off += 4;
    wr32be(out + off, sm); off += 4;
    /* off = 16 */

    /* --- T2: OSGetProgressiveMode with flags=0 --- */
    gc_sram_flags = 0;
    gc_sram_unlock_calls = 0;
    u32 prog0 = OSGetProgressiveMode();
    wr32be(out + off, 0x5150A002u); off += 4; /* tag */
    wr32be(out + off, prog0); off += 4;
    wr32be(out + off, gc_sram_unlock_calls); off += 4;
    /* off = 28 */

    /* --- T3: OSGetProgressiveMode with flags=0x80 (progressive on) --- */
    gc_sram_flags = 0x80;
    gc_sram_unlock_calls = 0;
    u32 prog1 = OSGetProgressiveMode();
    wr32be(out + off, 0x5150A003u); off += 4; /* tag */
    wr32be(out + off, prog1); off += 4;
    wr32be(out + off, gc_sram_unlock_calls); off += 4;
    /* off = 40 */

    /* --- T4: OSGetProgressiveMode with flags=0xFF --- */
    gc_sram_flags = 0xFF;
    gc_sram_unlock_calls = 0;
    u32 prog2 = OSGetProgressiveMode();
    wr32be(out + off, 0x5150A004u); off += 4; /* tag */
    wr32be(out + off, prog2); off += 4;
    wr32be(out + off, gc_sram_unlock_calls); off += 4;
    /* off = 52 */

    /* --- T5: OSGetTick (3 calls, each advances counter) --- */
    gc_os_tick_counter = 0;
    u32 t1 = OSGetTick();
    u32 t2 = OSGetTick();
    u32 t3 = OSGetTick();
    wr32be(out + off, 0x5150A005u); off += 4; /* tag */
    wr32be(out + off, t1); off += 4;
    wr32be(out + off, t2); off += 4;
    wr32be(out + off, t3); off += 4;
    /* off = 68 = 0x44 */

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
