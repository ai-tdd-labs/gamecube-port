#include <stdint.h>
#include <string.h>

#include "src/sdk_port/card/card_bios.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef uint16_t u16;

static inline void wr32be(volatile u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static inline u64 expected_serial_from_work_area(const u8 *serial)
{
    u64 code = 0;
    for (u32 i = 0; i < 4u; i++) {
        u64 piece = 0;
        for (u32 b = 0; b < 8u; b++) {
            piece = (piece << 8) | (u64)serial[i * 8u + b];
        }
        code ^= piece;
    }
    return code;
}

static inline void write_block_u32_word(u32 slot, u32 byte_offset, u32 value)
{
    ((u32 *)&gc_card_block[slot])[byte_offset >> 2u] = value;
}

static inline void write_row(volatile u8 *out, u32 *off, u32 tag, s32 rc, u64 serial, s32 block_result)
{
    wr32be(out + (*off) * 4u, tag);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)rc);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)(serial >> 32u));
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)serial);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)block_result);
    (*off)++;
}

s32 oracle_CARDGetSerialNo(s32 chan, u64* serialNo);

extern GcCardControl gc_card_block[GC_CARD_CHANS];

static void reset_card_state(void)
{
    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = 0;

    // Oracle control-block checks are done via legacy offsets from the original SDK
    // layout in this test build. Keep both canonical and offset-based copies in sync
    // to avoid host-vs-target ABI drift.
    write_block_u32_word(0, 12u, 0x80000000u);
    write_block_u32_word(0, 16u, 1u);
}

int main(void)
{
    volatile u8 *out = (volatile u8 *)0x80300000u;
    volatile u8 *mem = (volatile u8 *)0x80400000u;
    memset((void *)out, 0, 0x100u);
    memset((void *)mem, 0, 0x100u);

    u32 off = 0u;
    s32 rc = 0;
    u64 serial = 0xDEADBEEFu;

    for (u32 i = 0; i < 32u; i++) {
        mem[i] = (u8)i;
    }
    gc_card_block[0].work_area = (u32)(uintptr_t)mem;

    wr32be(out + off * 4u, 0x43534E30u);
    off++;
    wr32be(out + off * 4u, 0u);
    off++;

    rc = oracle_CARDGetSerialNo(-1, &serial);
    write_row(out, &off, 0x43534E31u, rc, serial, gc_card_block[0].result);

    gc_card_block[0].attached = 0;
    gc_card_block[0].work_area = (u32)(uintptr_t)mem;
    serial = 0x1234567800000000uLL;
    rc = oracle_CARDGetSerialNo(0, &serial);
    write_row(out, &off, 0x43534E32u, rc, serial, gc_card_block[0].result);

    reset_card_state();
    gc_card_block[0].work_area = (u32)(uintptr_t)mem;
    gc_card_block[0].result = -1;
    serial = 0xCAFEBABE00000000uLL;
    rc = oracle_CARDGetSerialNo(0, &serial);
    write_row(out, &off, 0x43534E33u, rc, serial, gc_card_block[0].result);

    reset_card_state();
    gc_card_block[0].work_area = (u32)(uintptr_t)mem;
    serial = 0xFFFFFFFFFFFFFFFFuLL;
    rc = oracle_CARDGetSerialNo(0, 0);
    write_row(out, &off, 0x43534E34u, rc, serial, gc_card_block[0].result);

    reset_card_state();
    gc_card_block[0].work_area = (u32)(uintptr_t)mem;
    for (u32 i = 0; i < 32u; i++) {
        mem[i] = (u8)(0x80u + i);
    }
    serial = 0x0123456789000000uLL;
    rc = oracle_CARDGetSerialNo(0, &serial);
    write_row(out, &off, 0x43534E35u, rc, serial, gc_card_block[0].result);

    wr32be(out + 0xF0u, 0x43534E56u);
    wr32be(out + 0xF4u, off);

    while (1) {}
    return 0;
}
