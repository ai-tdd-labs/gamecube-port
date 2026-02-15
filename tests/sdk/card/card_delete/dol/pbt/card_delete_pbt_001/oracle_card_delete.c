#include <stdint.h>
#include <string.h>

#include "src/sdk_port/card/card_bios.h"
#include "src/sdk_port/card/card_dir.h"
#include "src/sdk_port/card/card_fat.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

typedef void (*CARDCallback)(s32 chan, s32 result);

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BUSY = -1,
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_NOFILE = -4,
    CARD_RESULT_NOPERM = -10,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_FATAL_ERROR = -128,
};

enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };
enum { CARD_NUM_SYSTEM_BLOCK = 5u };

GcCardControl gc_card_block[GC_CARD_CHANS];

static inline void wr16be(u8* p, u16 v) { p[0] = (u8)(v >> 8u); p[1] = (u8)v; }
static inline u16 rd16be(const u8* p) { return (u16)(((u16)p[0] << 8u) | (u16)p[1]); }
static inline void wr32be(u8* p, u32 v)
{
    p[0] = (u8)(v >> 24u);
    p[1] = (u8)(v >> 16u);
    p[2] = (u8)(v >> 8u);
    p[3] = (u8)v;
}

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
        u16 v = rd16be(ptr + i * 2u);
        cs = (u16)(cs + v);
        csi = (u16)(csi + (u16)~v);
    }
    if (cs == 0xFFFFu) cs = 0;
    if (csi == 0xFFFFu) csi = 0;
    *checksum = cs;
    *checksumInv = csi;
}

static inline u16 fat_get(const u8* fat, u16 idx) { return rd16be(fat + (u32)idx * 2u); }
static inline void fat_set(u8* fat, u16 idx, u16 v) { wr16be(fat + (u32)idx * 2u, v); }

static void init_dir(u8* dir, u16 checkCode)
{
    memset(dir, 0xFFu, CARD_SYSTEM_BLOCK_SIZE);
    u8* chk = dir + (u32)PORT_CARD_MAX_FILE * (u32)PORT_CARD_DIR_SIZE;
    wr16be(chk + (PORT_CARD_DIR_SIZE - 6u), checkCode);
    u16 cs = 0, csi = 0;
    card_checksum_u16be(dir, CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 4u), cs);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 2u), csi);
}

static void init_fat(u8* fat, u16 checkCode, u16 cBlock)
{
    memset(fat, 0x00u, CARD_SYSTEM_BLOCK_SIZE);
    for (u16 b = (u16)CARD_NUM_SYSTEM_BLOCK; b < cBlock; b++) {
        fat_set(fat, b, PORT_CARD_FAT_AVAIL);
    }
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKCODE, checkCode);
    fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, (u16)(cBlock - (u16)CARD_NUM_SYSTEM_BLOCK));
    fat_set(fat, (u16)PORT_CARD_FAT_LASTSLOT, (u16)((u16)CARD_NUM_SYSTEM_BLOCK - 1u));

    u16 cs = 0, csi = 0;
    card_checksum_u16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUM, cs);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUMINV, csi);
}

static void seed_entry(u8* dir, u32 fileNo, const u8* id6, const char* name, u16 startBlock)
{
    u8* ent = dir + fileNo * (u32)PORT_CARD_DIR_SIZE;
    memcpy(ent + PORT_CARD_DIR_OFF_GAMENAME, id6, 4);
    memcpy(ent + PORT_CARD_DIR_OFF_COMPANY, id6 + 4, 2);
    memset(ent + PORT_CARD_DIR_OFF_FILENAME, 0, PORT_CARD_FILENAME_MAX);
    for (u32 i = 0; i < (u32)PORT_CARD_FILENAME_MAX; i++) {
        ent[PORT_CARD_DIR_OFF_FILENAME + i] = (u8)name[i];
        if (name[i] == '\0') break;
    }
    wr16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK, startBlock);
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

static int card_compare_filename(const u8* entry, const char* file_name)
{
    for (u32 i = 0; i < (u32)PORT_CARD_FILENAME_MAX; i++) {
        char a = (char)entry[PORT_CARD_DIR_OFF_FILENAME + i];
        char b = file_name[i];
        if (a != b) return 0;
        if (a == '\0') return 1;
    }
    return file_name[PORT_CARD_FILENAME_MAX] == '\0';
}

s32 __CARDAccess(const GcCardControl* card, const void* dirEntry)
{
    const u8* entry = (const u8*)dirEntry;
    if (!card || !entry) return CARD_RESULT_FATAL_ERROR;
    if (entry[PORT_CARD_DIR_OFF_GAMENAME] == 0xFFu) return CARD_RESULT_NOFILE;
    if (memcmp(entry + PORT_CARD_DIR_OFF_GAMENAME, card->id, 4) == 0 &&
        memcmp(entry + PORT_CARD_DIR_OFF_COMPANY, card->id + 4, 2) == 0) {
        return CARD_RESULT_READY;
    }
    return CARD_RESULT_NOPERM;
}

s32 __CARDGetFileNo(const GcCardControl* card, const char* fileName, s32* pfileNo)
{
    if (!card || !fileName || !pfileNo) return CARD_RESULT_FATAL_ERROR;
    if (!card->attached || !card->current_dir_ptr) return CARD_RESULT_NOCARD;

    const u8* dir = (const u8*)(uintptr_t)card->current_dir_ptr;
    for (s32 fileNo = 0; fileNo < (s32)PORT_CARD_MAX_FILE; fileNo++) {
        const u8* ent = dir + (u32)fileNo * (u32)PORT_CARD_DIR_SIZE;
        s32 access = __CARDAccess(card, ent);
        if (access < 0) continue;
        if (card_compare_filename(ent, fileName)) {
            *pfileNo = fileNo;
            return CARD_RESULT_READY;
        }
    }
    return CARD_RESULT_NOFILE;
}

s32 __CARDIsOpened(const GcCardControl* card, s32 fileNo) { (void)card; (void)fileNo; return 0; }

s32 __CARDUpdateDir(s32 chan, CARDCallback callback)
{
    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) return CARD_RESULT_NOCARD;
    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    if (!dir) return CARD_RESULT_BROKEN;

    u8* chk = dir + (u32)PORT_CARD_MAX_FILE * (u32)PORT_CARD_DIR_SIZE;
    u16 cc = rd16be(chk + (PORT_CARD_DIR_SIZE - 6u));
    cc = (u16)(cc + 1u);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 6u), cc);

    u16 cs = 0, csi = 0;
    card_checksum_u16be(dir, CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 4u), cs);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 2u), csi);

    __CARDPutControlBlock(card, CARD_RESULT_READY);
    if (callback) callback(chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

s32 __CARDUpdateFatBlock(s32 chan, u16* fat_u16, CARDCallback callback)
{
    u8* fat = (u8*)fat_u16;
    if (!fat) return CARD_RESULT_FATAL_ERROR;
    u16 cc = fat_get(fat, (u16)PORT_CARD_FAT_CHECKCODE);
    cc = (u16)(cc + 1u);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKCODE, cc);

    u16 cs = 0, csi = 0;
    card_checksum_u16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUM, cs);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUMINV, csi);

    if (callback) callback(chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

static int card_is_valid_block_no(const GcCardControl* card, u16 iBlock)
{
    return ((u16)CARD_NUM_SYSTEM_BLOCK <= iBlock && (u32)iBlock < (u32)card->cblock);
}

s32 __CARDFreeBlock(s32 chan, u16 nBlock, CARDCallback callback)
{
    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) return CARD_RESULT_NOCARD;
    u8* fat = (u8*)(uintptr_t)card->current_fat_ptr;
    if (!fat) return CARD_RESULT_BROKEN;

    while (nBlock != 0xFFFFu) {
        if (!card_is_valid_block_no(card, nBlock)) return CARD_RESULT_BROKEN;
        u16 next = fat_get(fat, nBlock);
        fat_set(fat, nBlock, PORT_CARD_FAT_AVAIL);

        u16 fb = fat_get(fat, (u16)PORT_CARD_FAT_FREEBLOCKS);
        fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, (u16)(fb + 1u));

        nBlock = next;
    }

    return __CARDUpdateFatBlock(chan, (u16*)fat, callback);
}

static void DeleteCallback(s32 chan, s32 result)
{
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;

    if (result < 0) goto error;

    result = __CARDFreeBlock(chan, (u16)card->startBlock, callback);
    if (result < 0) goto error;
    return;

error:
    __CARDPutControlBlock(card, result);
    if (callback) callback(chan, result);
}

s32 CARDFastDeleteAsync(s32 chan, s32 fileNo, CARDCallback callback)
{
    if (fileNo < 0 || PORT_CARD_MAX_FILE <= fileNo) {
        return CARD_RESULT_FATAL_ERROR;
    }

    GcCardControl* card;
    s32 result = __CARDGetControlBlock(chan, &card);
    if (result < 0) return result;

    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    if (!dir) return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);

    u8* ent = dir + (u32)fileNo * (u32)PORT_CARD_DIR_SIZE;
    result = __CARDAccess(card, ent);
    if (result < 0) return __CARDPutControlBlock(card, result);
    if (__CARDIsOpened(card, fileNo)) return __CARDPutControlBlock(card, CARD_RESULT_BUSY);

    card->startBlock = (u32)rd16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK);
    memset(ent, 0xFFu, PORT_CARD_DIR_SIZE);

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    result = __CARDUpdateDir(chan, DeleteCallback);
    if (result < 0) __CARDPutControlBlock(card, result);
    return result;
}

s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback)
{
    GcCardControl* card;
    s32 fileNo;
    s32 result;

    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) return result;
    result = __CARDGetFileNo(card, fileName, &fileNo);
    if (result < 0) return __CARDPutControlBlock(card, result);
    if (__CARDIsOpened(card, fileNo)) return __CARDPutControlBlock(card, CARD_RESULT_BUSY);

    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    if (!dir) return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    u8* ent = dir + (u32)fileNo * (u32)PORT_CARD_DIR_SIZE;
    card->startBlock = (u32)rd16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK);
    memset(ent, 0xFFu, PORT_CARD_DIR_SIZE);

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    result = __CARDUpdateDir(chan, DeleteCallback);
    if (result < 0) __CARDPutControlBlock(card, result);
    return result;
}

// Simple sync stubs for DOL tests.
void __CARDSyncCallback(s32 chan, s32 result) { (void)chan; (void)result; }
s32 __CARDSync(s32 chan) { (void)chan; return CARD_RESULT_READY; }

s32 CARDDelete(s32 chan, const char* fileName)
{
    s32 result = CARDDeleteAsync(chan, fileName, __CARDSyncCallback);
    if (result < 0) return result;
    return __CARDSync(chan);
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result) { (void)chan; g_cb_calls++; g_cb_last = result; }

void oracle_card_delete_pbt_001_suite(u8* out)
{
    memset(out, 0, 0x200u);

    u8* dir = (u8*)0x80400000u;
    u8* fat = (u8*)0x80402000u;

    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].cblock = 64;
    memcpy(gc_card_block[0].id, "GAMEC0", 6);

    init_dir(dir, 0);
    init_fat(fat, 0, 64);

    // Seed two files: fileNo=0 for fast-delete, fileNo=1 for name-delete.
    seed_entry(dir, 0, (const u8*)gc_card_block[0].id, "fast", 5);
    seed_entry(dir, 1, (const u8*)gc_card_block[0].id, "name", 7);
    // Seed a third file with wrong IDs for NOPERM case.
    const u8 other_id6[6] = {'B','A','D','0','0','0'};
    seed_entry(dir, 2, other_id6, "nope", 9);

    // Two chains: 5->6->FFFF and 7->8->FFFF.
    fat_set(fat, 5, 6);
    fat_set(fat, 6, 0xFFFFu);
    fat_set(fat, 7, 8);
    fat_set(fat, 8, 0xFFFFu);
    fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, (u16)(fat_get(fat, (u16)PORT_CARD_FAT_FREEBLOCKS) - 4u));

    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    u32 w = 0;
    wr32be(out + w, 0x43445230u); w += 4; // CDR0

    // Case A: CARDFastDeleteAsync success.
    g_cb_calls = 0;
    g_cb_last = 0;
    s32 rc = CARDFastDeleteAsync(0, 0, cb_record);
    wr32be(out + w, 0x43445231u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, (u32)gc_card_block[0].result); w += 4;
    wr32be(out + w, (u32)gc_card_block[0].startBlock); w += 4;
    wr32be(out + w, fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE)); w += 4;

    // Case B: CARDDeleteAsync success (by name).
    g_cb_calls = 0;
    g_cb_last = 0;
    rc = CARDDeleteAsync(0, "name", cb_record);
    wr32be(out + w, 0x43445232u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, (u32)gc_card_block[0].result); w += 4;
    wr32be(out + w, (u32)gc_card_block[0].startBlock); w += 4;
    wr32be(out + w, fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE)); w += 4;

    // Case C: CARDFastDeleteAsync invalid fileNo (no state change expected).
    u32 h_dir_before = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);
    u32 h_fat_before = fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE);
    g_cb_calls = 0;
    rc = CARDFastDeleteAsync(0, -1, cb_record);
    wr32be(out + w, 0x43445233u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, h_dir_before); w += 4;
    wr32be(out + w, h_fat_before); w += 4;

    // Case D: CARDFastDeleteAsync NOPERM (no state change expected).
    h_dir_before = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);
    h_fat_before = fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE);
    g_cb_calls = 0;
    rc = CARDFastDeleteAsync(0, 2, cb_record);
    wr32be(out + w, 0x43445234u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, h_dir_before); w += 4;
    wr32be(out + w, h_fat_before); w += 4;

    // Case E: CARDDeleteAsync NOFILE (no state change expected).
    h_dir_before = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);
    h_fat_before = fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE);
    g_cb_calls = 0;
    rc = CARDDeleteAsync(0, "missing", cb_record);
    wr32be(out + w, 0x43445235u); w += 4;
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, h_dir_before); w += 4;
    wr32be(out + w, h_fat_before); w += 4;
}
