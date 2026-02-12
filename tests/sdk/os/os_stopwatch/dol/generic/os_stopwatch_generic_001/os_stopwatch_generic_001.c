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

void OSInitStopwatch(OSStopwatch *sw, char *name);
void OSStartStopwatch(OSStopwatch *sw);
void OSStopStopwatch(OSStopwatch *sw);
long long OSCheckStopwatch(OSStopwatch *sw);
void OSResetStopwatch(OSStopwatch *sw);

/* OSReport is provided by libogc on DOL. */
int OSReport(const char *msg, ...);

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

/* Write u64 as two big-endian u32s (high word first). */
static inline void wr64be(volatile uint8_t *p, u64 v) {
    wr32be(p,     (u32)(v >> 32));
    wr32be(p + 4, (u32)(v & 0xFFFFFFFFu));
}

/* Dump full stopwatch state (40 bytes). */
static inline void dump_sw(volatile uint8_t *p, const OSStopwatch *sw) {
    wr64be(p + 0,  sw->total);
    wr32be(p + 8,  sw->hits);
    wr32be(p + 12, sw->running);
    wr64be(p + 16, sw->last);
    wr64be(p + 24, sw->min);
    wr64be(p + 32, sw->max);
}

int main(void) {
    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x100; i++) out[i] = 0;

    u32 off = 0;
    OSStopwatch sw;
    long long check;

    /* --- T1: Init --- */
    OSInitStopwatch(&sw, "test");
    wr32be(out + off, 0x5710A001u); off += 4; /* tag */
    dump_sw(out + off, &sw); off += 40;
    /* off = 44 */

    /* --- T2: Start (OSGetTime returns 1) --- */
    OSStartStopwatch(&sw);
    wr32be(out + off, 0x5710A002u); off += 4;
    dump_sw(out + off, &sw); off += 40;
    /* off = 88 */

    /* --- T3: Stop (OSGetTime returns 2, interval=1) --- */
    OSStopStopwatch(&sw);
    wr32be(out + off, 0x5710A003u); off += 4;
    dump_sw(out + off, &sw); off += 40;
    /* off = 132 */

    /* --- T4: Start again (OSGetTime returns 3) --- */
    OSStartStopwatch(&sw);
    wr32be(out + off, 0x5710A004u); off += 4;
    dump_sw(out + off, &sw); off += 40;
    /* off = 176 */

    /* --- T5: Stop again (OSGetTime returns 4, interval=1, total=2) --- */
    OSStopStopwatch(&sw);
    wr32be(out + off, 0x5710A005u); off += 4;
    dump_sw(out + off, &sw); off += 40;
    /* off = 220 */

    /* --- T6: Check while stopped --- */
    check = OSCheckStopwatch(&sw);
    wr32be(out + off, 0x5710A006u); off += 4;
    wr64be(out + off, (u64)check); off += 8;
    /* off = 232 */

    /* --- T7: Reset --- */
    OSResetStopwatch(&sw);
    wr32be(out + off, 0x5710A007u); off += 4;
    dump_sw(out + off, &sw); off += 40;
    /* off = 276 = 0x114 */

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
