#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "sdk_port/card/card_bios.h"
#include "sdk_port/card/card_dir.h"
#include "sdk_port/card/card_fat.h"

#include "dolphin/OSRtcPriv.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_ENCODING = -13,
    CARD_RESULT_BUSY = -1,
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_FATAL_ERROR = -128,
};

enum {
    CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u,
    CARD_NUM_SYSTEM_BLOCK = 5u,
};

s32 CARDCheck(s32 chan);
s32 CARDCheckExAsync(s32 chan, s32* xferBytes, void (*callback)(s32, s32));
s32 CARDCheckAsync(s32 chan, void (*callback)(s32, s32));

static u32 fnv1a32(const u8* p, u32 n)
{
    u32 h = 2166136261u;
    for (u32 i = 0u; i < n; i++) {
        h ^= (u32)p[i];
        h *= 16777619u;
    }
    return h;
}

static inline void wr16be(u8* p, u16 v)
{
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static inline u16 rd16be(const u8* p)
{
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static inline u32 mem_addr(const u8* base_actual, u32 base_addr, const u8* ptr)
{
    if (!ptr) {
        return 0u;
    }
    return (u32)(base_addr + (u32)((uintptr_t)ptr - (uintptr_t)base_actual));
}

static void card_checksum_u16be(const u8* ptr, u32 length, u16* checksum, u16* checksumInv)
{
    u32 n = length / 2u;
    u16 cs = 0;
    u16 csi = 0;
    for (u32 i = 0u; i < n; i++) {
        u16 v = rd16be(ptr + i * 2u);
        cs = (u16)(cs + v);
        csi = (u16)(csi + (u16)~v);
    }
    if (cs == 0xFFFFu) cs = 0;
    if (csi == 0xFFFFu) csi = 0;
    *checksum = cs;
    *checksumInv = csi;
}

static inline void fat_set(u8* fat, u16 idx, u16 v)
{
    wr16be(fat + (u32)idx * 2u, v);
}

static void make_dir_block(u8* base, s16 checkCode, u8 seed)
{
    for (u32 i = 0u; i < (u32)CARD_SYSTEM_BLOCK_SIZE; i++) {
        base[i] = (u8)(seed ^ (u8)i);
    }
    for (u32 i = 0u; i < (u32)PORT_CARD_MAX_FILE; i++) {
        base[i * (u32)PORT_CARD_DIR_SIZE] = 0xFFu;
    }
    u8* chk = base + (u32)PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE;
    wr16be(chk + (PORT_CARD_DIR_SIZE - 6u), (u16)checkCode);
    u16 cs = 0u, csi = 0u;
    card_checksum_u16be(base, (u32)CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 4u), cs);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 2u), csi);
}

static void make_fat_block(u8* base, u16 checkCode, u16 cBlock)
{
    memset(base, 0x00u, (size_t)CARD_SYSTEM_BLOCK_SIZE);
    for (u16 b = CARD_NUM_SYSTEM_BLOCK; b < cBlock; b++) {
        fat_set(base, b, PORT_CARD_FAT_AVAIL);
    }
    fat_set(base, PORT_CARD_FAT_CHECKCODE, checkCode);
    fat_set(base, PORT_CARD_FAT_FREEBLOCKS, (u16)(cBlock - CARD_NUM_SYSTEM_BLOCK));

    u16 cs = 0u, csi = 0u;
    card_checksum_u16be(base + (u32)PORT_CARD_FAT_CHECKCODE * 2u, (u32)CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
    fat_set(base, PORT_CARD_FAT_CHECKSUM, cs);
    fat_set(base, PORT_CARD_FAT_CHECKSUMINV, csi);
}

static void make_id_block(u8* base, u16 size_u16, u16 encode, const u8* flash_id12)
{
    memset(base, 0, 512u);
    for (u32 i = 0u; i < 12u; i++) {
        base[i] = flash_id12[i];
    }
    wr16be(base + 32u, 0u);
    wr16be(base + 34u, size_u16);
    wr16be(base + 36u, encode);
    u16 cs = 0u, csi = 0u;
    card_checksum_u16be(base, 512u - 4u, &cs, &csi);
    wr16be(base + 508u, cs);
    wr16be(base + 510u, csi);
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result)
{
    (void)chan;
    g_cb_calls++;
    g_cb_last = result;
}

static void reset_card_state(u8* work, u16 size_u16, s32 cBlock)
{
    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].size_u16 = size_u16;
    gc_card_block[0].cblock = (u32)cBlock;
    gc_card_block[0].work_area = (uintptr_t)work;
    gc_card_block[0].current_dir_ptr = 0u;
    gc_card_block[0].current_fat_ptr = 0u;
    gc_card_block[0].sector_size = CARD_SYSTEM_BLOCK_SIZE;
    memset(gc_card_block[0].id, 0, sizeof(gc_card_block[0].id));
}

static void fill_sram_for_verify(const u8 flash_id12[12])
{
    __OSInitSram();
    OSSramEx* sram = __OSLockSramEx();
    if (!sram) {
        die("OSSram init failed");
    }
    memcpy(sram->flashID[0], flash_id12, 12u);
    u8 sum = 0u;
    for (u32 i = 0u; i < 12u; i++) {
        sum = (u8)(sum + sram->flashID[0][i]);
    }
    sram->flashIDCheckSum[0] = (u8)~sum;
    sram->flashIDCheckSum[1] = (u8)~sum;
    (void)__OSUnlockSramEx(0);
}

static void dump_case(
    u8* out,
    u32* w,
    u32 tag,
    s32 rc,
    u32 xferBytes,
    s32 cbCalls,
    s32 cbLast
)
{
    const GcCardControl* card = &gc_card_block[0];
    const u8* work = (const u8*)(uintptr_t)card->work_area;
    const u32 mem_base = 0x81000000u;

    wr32be(out + *w * 4u, tag); (*w)++;
    wr32be(out + *w * 4u, (u32)rc); (*w)++;
    wr32be(out + *w * 4u, xferBytes); (*w)++;
    wr32be(out + *w * 4u, (u32)card->result); (*w)++;
    wr32be(out + *w * 4u, mem_addr(work, mem_base, (const u8*)card->current_dir_ptr)); (*w)++;
    wr32be(out + *w * 4u, mem_addr(work, mem_base, (const u8*)card->current_fat_ptr)); (*w)++;
    wr32be(out + *w * 4u, (u32)cbCalls); (*w)++;
    wr32be(out + *w * 4u, (u32)cbLast); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x0000u, 512u)); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x2000u, (u32)CARD_SYSTEM_BLOCK_SIZE)); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x4000u, (u32)CARD_SYSTEM_BLOCK_SIZE)); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x6000u, (u32)CARD_SYSTEM_BLOCK_SIZE)); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x8000u, (u32)CARD_SYSTEM_BLOCK_SIZE)); (*w)++;
}

const char* gc_scenario_label(void) { return "CARDCheck/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_check_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram)
{
    const char* limit_env = getenv("CARD_CHECK_CASE_LIMIT");
    int limit = 7;
    if (limit_env && *limit_env) {
        char* endp = 0;
        long lv = strtol(limit_env, &endp, 0);
        if (endp && *endp == 0) {
            if (lv > 0 && lv <= 7) {
                limit = (int)lv;
            }
        }
    }

    u8* out = gc_ram_ptr(ram, 0x80300000u, 0x200u);
    if (!out) die("gc_ram_ptr(out) failed");
    memset(out, 0, 0x200u);
    u32 off = 0u;
    wr32be(out + (off * 4u), 0x43434B30u); off++;
    wr32be(out + (off * 4u), 0u); off++;

    u8* work = gc_ram_ptr(ram, 0x80400000u, 5u * (u32)CARD_SYSTEM_BLOCK_SIZE);
    if (!work) die("gc_ram_ptr(work) failed");
    memset(work, 0, 5u * (u32)CARD_SYSTEM_BLOCK_SIZE);

    static const u8 flash0[12] = {
        0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u,
        0x16u, 0x17u, 0x18u, 0x19u, 0x1Au, 0x1Bu
    };
    const s32 cBlock = 64;

    fill_sram_for_verify(flash0);

    // Case 0x31: CARDCheck() success path.
    memset(work, 0, 5u * (u32)CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, (u16)cBlock);
    make_fat_block(work + 0x8000u, 2u, (u16)cBlock);
    g_cb_calls = 0;
    g_cb_last = 0;
    s32 rc = CARDCheck(0);
    dump_case(out, &off, 0x43434B31u, rc, 0u, g_cb_calls, g_cb_last);
    if (limit <= 1) {
        return;
    }

    // Case 0x32: one bad dir, update dir path.
    memset(work, 0, 5u * (u32)CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, (u16)cBlock);
    make_fat_block(work + 0x8000u, 2u, (u16)cBlock);
    wr16be(work + 0x2000u + (u32)PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE + (PORT_CARD_DIR_SIZE - 2u), 0x1234u);
    s32 xfer = 0u;
    g_cb_calls = 0;
    g_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &off, 0x43434B32u, rc, (u32)xfer, g_cb_calls, g_cb_last);
    if (limit <= 2) {
        return;
    }

    // Case 0x33: one bad FAT, update fat path.
    memset(work, 0, 5u * (u32)CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, (u16)cBlock);
    make_fat_block(work + 0x8000u, 2u, (u16)cBlock);
    wr16be(work + 0x6000u + (u32)PORT_CARD_FAT_CHECKSUM * 2u, 0x1234u);
    xfer = 0u;
    g_cb_calls = 0;
    g_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &off, 0x43434B33u, rc, (u32)xfer, g_cb_calls, g_cb_last);
    if (limit <= 3) {
        return;
    }

    // Case 0x34: two errors -> BROKEN.
    memset(work, 0, 5u * (u32)CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, (u16)cBlock);
    make_fat_block(work + 0x8000u, 2u, (u16)cBlock);
    wr16be(work + 0x2000u + (u32)PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE + (PORT_CARD_DIR_SIZE - 2u), 0x1234u);
    wr16be(work + 0x6000u + (u32)PORT_CARD_FAT_CHECKSUMINV * 2u, 0x1234u);
    xfer = 0u;
    g_cb_calls = 0;
    g_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &off, 0x43434B34u, rc, (u32)xfer, g_cb_calls, g_cb_last);
    if (limit <= 4) {
        return;
    }

    // Case 0x35: encoding mismatch -> ENCODING.
    memset(work, 0, 5u * (u32)CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 1u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, (u16)cBlock);
    make_fat_block(work + 0x8000u, 2u, (u16)cBlock);
    xfer = 0u;
    g_cb_calls = 0;
    g_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &off, 0x43434B35u, rc, (u32)xfer, g_cb_calls, g_cb_last);
    if (limit <= 5) {
        return;
    }

    // Case 0x36: attached false -> NOCARD.
    memset(work, 0, 5u * (u32)CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, cBlock);
    gc_card_block[0].attached = 0;
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, (u16)cBlock);
    make_fat_block(work + 0x8000u, 2u, (u16)cBlock);
    xfer = 0u;
    g_cb_calls = 0;
    g_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &off, 0x43434B36u, rc, (u32)xfer, g_cb_calls, g_cb_last);

    if (limit <= 6) {
        return;
    }

    // Case 0x37: orphan repair path updates FAT entries + FREEBLOCKS + checksums, then updates FAT block.
    memset(work, 0, 5u * (u32)CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, (u16)cBlock);
    make_fat_block(work + 0x8000u, 2u, (u16)cBlock);
    // Force a non-AVAIL entry that no directory references (map[]==0), so CARDCheck repairs it to AVAIL.
    fat_set(work + 0x6000u, 10u, 0xFFFFu);

    xfer = 0u;
    g_cb_calls = 0;
    g_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &off, 0x43434B37u, rc, (u32)xfer, g_cb_calls, g_cb_last);
}
