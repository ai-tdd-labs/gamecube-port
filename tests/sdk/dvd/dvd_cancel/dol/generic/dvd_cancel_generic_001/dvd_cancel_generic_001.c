#include <stdint.h>
#include "src/sdk_port/gc_mem.h"
#include "src/sdk_port/sdk_state.h"

typedef uint32_t u32;
typedef int32_t  s32;

typedef struct {
    volatile s32 state;
} DVDCommandBlock;

s32 DVDCancel(DVDCommandBlock *block);

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}

int main(void) {
    gc_mem_set(0x80000000u, 0x01800000u, (uint8_t *)0x80000000u);
    gc_sdk_state_reset();

    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x40; i++) out[i] = 0;

    // Test 1: Cancel NULL block (returns -1).
    s32 r1 = DVDCancel((DVDCommandBlock *)0);

    // Test 2: Cancel idle block (state=0, returns 0).
    DVDCommandBlock blk2;
    blk2.state = 0;
    s32 r2 = DVDCancel(&blk2);

    // Test 3: Cancel already-canceled block (state=10, returns 0).
    DVDCommandBlock blk3;
    blk3.state = 10;
    s32 r3 = DVDCancel(&blk3);

    // Test 4: Cancel finished block (state=-1, returns 0).
    DVDCommandBlock blk4;
    blk4.state = -1;
    s32 r4 = DVDCancel(&blk4);

    // Test 5: Cancel busy block (state=1, transitions to 10, returns 0).
    DVDCommandBlock blk5;
    blk5.state = 1;
    s32 r5 = DVDCancel(&blk5);
    s32 s5 = blk5.state;

    u32 off = 0;
    wr32be(out + off, 0xDEADBEEFu); off += 4;
    wr32be(out + off, (u32)r1); off += 4;
    wr32be(out + off, (u32)r2); off += 4;
    wr32be(out + off, (u32)blk2.state); off += 4;
    wr32be(out + off, (u32)r3); off += 4;
    wr32be(out + off, (u32)r4); off += 4;
    wr32be(out + off, (u32)r5); off += 4;
    wr32be(out + off, (u32)s5); off += 4;
    // Total: 8 * 4 = 0x20

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
