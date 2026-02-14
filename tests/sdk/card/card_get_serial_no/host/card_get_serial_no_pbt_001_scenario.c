#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_port/card/card_bios.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;

static inline u64 expected_serial_from_work_area(const u8* serial) {
    u64 code = 0;
    for (u32 i = 0; i < 4; i++) {
        u64 piece = 0;
        for (u32 b = 0; b < 8; b++) {
            piece = (piece << 8) | (u64)serial[i * 8u + b];
        }
        code ^= piece;
    }
    return code;
}

static inline void write_row(u8* out, u32* off, u32 tag, s32 rc, u64 serial) {
    wr32be(out + (*off) * 4u, tag);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)rc);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)(serial >> 32u));
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)serial);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)gc_card_block[0].result);
    (*off)++;
}

static void reset_card_state(void)
{
    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = 0;
}

const char* gc_scenario_label(void) {
    return "CARDGetSerialNo/pbt_001";
}

const char* gc_scenario_out_path(void) {
    return "../actual/card_get_serial_no_pbt_001.bin";
}

void gc_scenario_run(GcRam* ram) {
    u8* out = gc_ram_ptr(ram, 0x80300000u, 0x100u);
    if (!out) {
        die("gc_ram_ptr(out) failed");
    }

    u8* mem = gc_ram_ptr(ram, 0x80400000u, 0x100u);
    if (!mem) {
        die("gc_ram_ptr(mem) failed");
    }

    memset(out, 0, 0x100u);
    memset(mem, 0, 0x100u);

    reset_card_state();
    gc_card_block[0].work_area = (uintptr_t)mem;

    for (u32 i = 0; i < 32u; i++) {
        mem[i] = (u8)i;
    }

    u32 off = 0;
    u64 serial = 0xDEADBEEFu;
    s32 rc = 0;

    wr32be(out, 0x43534E30u); // "CSN0"
    off++;
    wr32be(out + off * 4u, 0x00000000u);
    off++;

    rc = CARDGetSerialNo(-1, &serial);
    write_row(out, &off, 0x43534E31u, rc, serial); // CSN1

    gc_card_block[0].attached = 0;
    gc_card_block[0].disk_id = 0;
    gc_card_block[0].work_area = (uintptr_t)mem;
    serial = 0x1234567800000000uLL;
    rc = CARDGetSerialNo(0, &serial);
    write_row(out, &off, 0x43534E32u, rc, serial); // CSN2

    reset_card_state();
    gc_card_block[0].work_area = (uintptr_t)mem;
    gc_card_block[0].result = -1;
    serial = 0xCAFEBABE00000000uLL;
    rc = CARDGetSerialNo(0, &serial);
    write_row(out, &off, 0x43534E33u, rc, serial); // CSN3

    reset_card_state();
    gc_card_block[0].work_area = (uintptr_t)mem;
    serial = 0xFFFFFFFFFFFFFFFFuLL;
    rc = CARDGetSerialNo(0, 0);
    write_row(out, &off, 0x43534E34u, rc, serial); // CSN4

    reset_card_state();
    gc_card_block[0].work_area = (uintptr_t)mem;
    for (u32 i = 0; i < 32u; i++) {
        mem[i] = (u8)(0x80u + i);
    }
    serial = 0x0123456789000000uLL;
    rc = CARDGetSerialNo(0, &serial);
    serial = expected_serial_from_work_area(mem);
    write_row(out, &off, 0x43534E35u, rc, serial); // CSN5

    // Footer: marker + word count so CI can sanity-check truncation.
    wr32be(out + 0xF0u, 0x43534E56u); // "CSNV"
    wr32be(out + 0xF4u, off);
}
