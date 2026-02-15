#include <stdint.h>
#include <string.h>

#include "src/sdk_port/card/card_bios.h"
#include "src/sdk_port/card/card_dir.h"
#include "src/sdk_port/card/card_fat.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;

typedef void (*CARDCallback)(s32 chan, s32 result);

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BUSY = -1,
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_FATAL_ERROR = -128,
};

enum { CARD_NUM_SYSTEM_BLOCK = 5u };
enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };
enum { TICKS_PER_SEC = (162000000u / 4u) };

GcCardControl gc_card_block[GC_CARD_CHANS];

static inline void wr16be(u8* p, u16 v) { p[0] = (u8)(v >> 8u); p[1] = (u8)v; }
static inline void wr32be(u8* p, u32 v)
{
    p[0] = (u8)(v >> 24u);
    p[1] = (u8)(v >> 16u);
    p[2] = (u8)(v >> 8u);
    p[3] = (u8)v;
}
static inline void wr64be(u8* p, u64 v)
{
    for (int i = 7; i >= 0; i--) {
        p[7 - i] = (u8)(v >> (u32)(i * 8));
    }
}
static inline u16 rd16be(const u8* p) { return (u16)(((u16)p[0] << 8u) | (u16)p[1]); }

static u32 fnv1a32(const u8* p, u32 n)
{
    u32 h = 2166136261u;
    for (u32 i = 0; i < n; i++) {
        h ^= (u32)p[i];
        h *= 16777619u;
    }
    return h;
}

static void card_checksum_u16be(const u8* ptr, u32 length, u16* checksum, u16* checksumInv)
{
    u32 n = length / 2u;
    u16 cs = 0, csi = 0;
    for (u32 i = 0; i < n; i++) {
        u16 v = (u16)(((u16)ptr[i * 2u] << 8u) | (u16)ptr[i * 2u + 1u]);
        cs = (u16)(cs + v);
        csi = (u16)(csi + (u16)~v);
    }
    if (cs == 0xFFFFu) cs = 0;
    if (csi == 0xFFFFu) csi = 0;
    *checksum = cs;
    *checksumInv = csi;
}

static u64 oracle_time_ticks(void)
{
    static u64 ticks;
    ticks += (u64)TICKS_PER_SEC;
    return ticks;
}

void __CARDDefaultApiCallback(s32 chan, s32 result) { (void)chan; (void)result; }

s32 __CARDGetControlBlock(s32 chan, GcCardControl** pcard)
{
    if (chan < 0 || chan >= GC_CARD_CHANS) return CARD_RESULT_FATAL_ERROR;
    GcCardControl* card = &gc_card_block[chan];
    if (!card->disk_id) return CARD_RESULT_FATAL_ERROR;
    if (!card->attached) return CARD_RESULT_NOCARD;
    if (card->result == CARD_RESULT_BUSY) return CARD_RESULT_BUSY;
    card->result = CARD_RESULT_BUSY;
    card->api_callback = 0u;
    if (pcard) *pcard = card;
    return CARD_RESULT_READY;
}

s32 __CARDPutControlBlock(GcCardControl* card, s32 result)
{
    if (!card) return result;
    if (card->attached || card->result == CARD_RESULT_BUSY) {
        card->result = result;
    }
    return result;
}

static void init_dir_block(u8* dir, u16 checkCode)
{
    memset(dir, 0xFFu, CARD_SYSTEM_BLOCK_SIZE);
    u8* chk = dir + (u32)PORT_CARD_MAX_FILE * (u32)PORT_CARD_DIR_SIZE;
    wr16be(chk + (PORT_CARD_DIR_SIZE - 6u), checkCode);
    u16 cs = 0, csi = 0;
    card_checksum_u16be(dir, CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 4u), cs);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 2u), csi);
}

static void init_fat_block(u8* fat, u16 checkCode, u16 cBlock)
{
    memset(fat, 0x00u, CARD_SYSTEM_BLOCK_SIZE);
    wr16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, checkCode);
    wr16be(fat + (u32)PORT_CARD_FAT_FREEBLOCKS * 2u, (u16)(cBlock - (u16)CARD_NUM_SYSTEM_BLOCK));
    wr16be(fat + (u32)PORT_CARD_FAT_LASTSLOT * 2u, (u16)((u16)CARD_NUM_SYSTEM_BLOCK - 1u));

    u16 cs = 0, csi = 0;
    card_checksum_u16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
    wr16be(fat + (u32)PORT_CARD_FAT_CHECKSUM * 2u, cs);
    wr16be(fat + (u32)PORT_CARD_FAT_CHECKSUMINV * 2u, csi);
}

s32 __CARDFormatRegionAsync(s32 chan, u16 encode, CARDCallback callback)
{
    GcCardControl* card;
    s32 result = __CARDGetControlBlock(chan, &card);
    if (result < 0) return result;

    u8* work = (u8*)(uintptr_t)card->work_area;
    if (!work) return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);

    memset(work, 0xFFu, CARD_SYSTEM_BLOCK_SIZE);
    u8* id = work;

    // serial[20], serial[24] are zeros in the host port model.
    wr32be(id + 20u, 0u);
    wr32be(id + 24u, 0u);

    u64 time = oracle_time_ticks();
    u64 rand = time;
    for (int i = 0; i < 12; i++) {
        rand = (rand * 1103515245u + 12345u) >> 16;
        id[(u32)i] = (u8)rand;
        rand = ((rand * 1103515245u + 12345u) >> 16) & 0x7FFFull;
    }

    // serial[28] viDTVStatus modeled as 0, serial[12] time.
    wr32be(id + 28u, 0u);
    wr64be(id + 12u, time);

    // deviceID=0, size, encode.
    wr16be(id + 32u, 0u);
    wr16be(id + 34u, card->size_u16);
    wr16be(id + 36u, encode);

    // Header checksum over 512 bytes minus final u32.
    u16 id_cs = 0, id_csi = 0;
    card_checksum_u16be(id, 512u - 4u, &id_cs, &id_csi);
    wr16be(id + 508u, id_cs);
    wr16be(id + 510u, id_csi);

    init_dir_block(work + (1u + 0u) * (u32)CARD_SYSTEM_BLOCK_SIZE, 0);
    init_dir_block(work + (1u + 1u) * (u32)CARD_SYSTEM_BLOCK_SIZE, 1);
    init_fat_block(work + (3u + 0u) * (u32)CARD_SYSTEM_BLOCK_SIZE, 0, (u16)card->cblock);
    init_fat_block(work + (3u + 1u) * (u32)CARD_SYSTEM_BLOCK_SIZE, 1, (u16)card->cblock);

    // Deterministic model: install current dir/fat immediately (copy alt block over primary).
    card->current_dir_ptr = (uintptr_t)(work + (1u + 0u) * (u32)CARD_SYSTEM_BLOCK_SIZE);
    memcpy((void*)card->current_dir_ptr, work + (1u + 1u) * (u32)CARD_SYSTEM_BLOCK_SIZE, CARD_SYSTEM_BLOCK_SIZE);
    card->current_fat_ptr = (uintptr_t)(work + (3u + 0u) * (u32)CARD_SYSTEM_BLOCK_SIZE);
    memcpy((void*)card->current_fat_ptr, work + (3u + 1u) * (u32)CARD_SYSTEM_BLOCK_SIZE, CARD_SYSTEM_BLOCK_SIZE);

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    CARDCallback cb = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;
    __CARDPutControlBlock(card, CARD_RESULT_READY);
    if (cb) cb(chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result) { (void)chan; g_cb_calls++; g_cb_last = result; }

void oracle_card_format_pbt_001_suite(u8* out)
{
    memset(out, 0, 0x200u);

    u8* work = (u8*)0x80400000u;
    const u32 work_size = (u32)CARD_NUM_SYSTEM_BLOCK * (u32)CARD_SYSTEM_BLOCK_SIZE;

    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].cblock = 64;
    gc_card_block[0].size_u16 = 0x0020u;
    gc_card_block[0].work_area = (uintptr_t)work;

    u32 w = 0;
    wr32be(out + w, 0x43465230u); w += 4; // CFR0

    // Case A: success.
    g_cb_calls = 0;
    g_cb_last = 0;
    s32 rc = __CARDFormatRegionAsync(0, 0x1234u, cb_record);

    u8* cur_dir = (u8*)(uintptr_t)gc_card_block[0].current_dir_ptr;
    u8* cur_fat = (u8*)(uintptr_t)gc_card_block[0].current_fat_ptr;
    u16 dir_cc = cur_dir ? rd16be(cur_dir + (u32)PORT_CARD_MAX_FILE * (u32)PORT_CARD_DIR_SIZE + (PORT_CARD_DIR_SIZE - 6u)) : 0;
    u16 fat_cc = cur_fat ? rd16be(cur_fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u) : 0;
    u16 id_encode = rd16be(work + 36u);

    wr32be(out + w, 0x43465231u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, (u32)gc_card_block[0].result); w += 4;
    wr32be(out + w, (u32)dir_cc); w += 4;
    wr32be(out + w, (u32)fat_cc); w += 4;
    wr32be(out + w, (u32)id_encode); w += 4;
    wr32be(out + w, fnv1a32(work, work_size)); w += 4;

    // Case B: broken (null work_area).
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].work_area = 0u;
    g_cb_calls = 0;
    rc = __CARDFormatRegionAsync(0, 0x1234u, cb_record);
    wr32be(out + w, 0x43465232u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
}
