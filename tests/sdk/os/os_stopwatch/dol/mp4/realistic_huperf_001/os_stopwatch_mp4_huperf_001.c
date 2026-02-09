#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;

typedef struct OSStopwatch {
    char *name;
    u64 total;
    u32 hits;
    u32 running;
    u64 last;
    u64 min;
    u64 max;
} OSStopwatch;

static u64 g_time;
static u64 OSGetTime(void) {
    // Deterministic monotonic counter for tests.
    return ++g_time;
}

static void OSInitStopwatch(OSStopwatch *sw, char *name) {
    sw->name = name;
    sw->total = 0;
    sw->hits = 0;
    sw->min = 0x00000000FFFFFFFFull;
    sw->max = 0;
    sw->running = 0;
    sw->last = 0;
}

static void OSStartStopwatch(OSStopwatch *sw) {
    sw->running = 1;
    sw->last = OSGetTime();
}

static void OSStopStopwatch(OSStopwatch *sw) {
    long long interval;
    if (sw->running != 0) {
        interval = (long long)(OSGetTime() - sw->last);
        sw->total += (u64)interval;
        sw->running = 0;
        sw->hits++;
        if (sw->max < (u64)interval) sw->max = (u64)interval;
        if ((u64)interval < sw->min) sw->min = (u64)interval;
    }
}

static long long OSCheckStopwatch(OSStopwatch *sw) {
    long long currTotal = (long long)sw->total;
    if (sw->running != 0) {
        currTotal += (long long)(OSGetTime() - sw->last);
    }
    return currTotal;
}

static void OSResetStopwatch(OSStopwatch *sw) {
    OSInitStopwatch(sw, sw->name);
}

static inline void gc_disable_interrupts(void) {
    u32 msr;
    __asm__ volatile("mfmsr %0" : "=r"(msr));
    msr &= ~0x8000u;
    __asm__ volatile("mtmsr %0; isync" : : "r"(msr));
}

static inline void gc_arm_decrementer_far_future(void) {
    __asm__ volatile(
        "lis 3,0x7fff\n"
        "ori 3,3,0xffff\n"
        "mtdec 3\n"
        :
        :
        : "r3");
}

static inline void gc_safepoint(void) {
    gc_disable_interrupts();
    gc_arm_decrementer_far_future();
}

int main(void) {
    gc_safepoint();

    OSStopwatch sw;
    OSInitStopwatch(&sw, (char*)"CPU");

    // MP4-like sequence: stop/reset/start on a fresh stopwatch.
    OSStopStopwatch(&sw);   // no-op
    OSResetStopwatch(&sw);  // resets fields
    OSStartStopwatch(&sw);
    long long t0 = OSCheckStopwatch(&sw);
    OSStopStopwatch(&sw);
    long long t1 = OSCheckStopwatch(&sw);

    // Write observables.
    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = (u32)sw.hits;
    *(volatile u32*)0x80300008 = (u32)sw.running;
    *(volatile u32*)0x8030000C = (u32)(sw.total & 0xFFFFFFFFu);
    *(volatile u32*)0x80300010 = (u32)(sw.min & 0xFFFFFFFFu);
    *(volatile u32*)0x80300014 = (u32)(sw.max & 0xFFFFFFFFu);
    *(volatile u32*)0x80300018 = (u32)(t0 & 0xFFFFFFFFu);
    *(volatile u32*)0x8030001C = (u32)(t1 & 0xFFFFFFFFu);

    while (1) gc_safepoint();
}
