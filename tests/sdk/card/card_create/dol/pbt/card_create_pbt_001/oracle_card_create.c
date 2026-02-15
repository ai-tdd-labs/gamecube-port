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
    CARD_RESULT_EXIST = -7,
    CARD_RESULT_NOENT = -8,
    CARD_RESULT_INSSPACE = -9,
    CARD_RESULT_FATAL_ERROR = -128,
    CARD_RESULT_NAMETOOLONG = -12,
};

enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };
enum { CARD_NUM_SYSTEM_BLOCK = 5u };
enum { TICKS_PER_SEC = (162000000u / 4u) };

GcCardControl gc_card_block[GC_CARD_CHANS];

static inline u16 rd16be(const u8* p) { return (u16)(((u16)p[0] << 8u) | (u16)p[1]); }
static inline void wr16be(u8* p, u16 v) { p[0] = (u8)(v >> 8u); p[1] = (u8)v; }
static inline void wr32be(u8* p, u32 v) { p[0] = (u8)(v >> 24u); p[1] = (u8)(v >> 16u); p[2] = (u8)(v >> 8u); p[3] = (u8)v; }

static u32 oracle_time_seconds(void)
{
    static u32 ticks;
    ticks += TICKS_PER_SEC;
    return ticks / TICKS_PER_SEC;
}

static void card_checksum_u16be(const u8* ptr, u32 length, u16* checksum, u16* checksumInv)
{
    u32 n = length / 2u;
    u16 cs = 0;
    u16 csi = 0;
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

static void oracle_check_sum(void* ptr, int length, u16* checksum, u16* checksumInv)
{
    card_checksum_u16be((const u8*)ptr, (u32)length, checksum, checksumInv);
}

s32 __CARDUpdateDir(s32 chan, CARDCallback callback)
{
    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) return CARD_RESULT_NOCARD;
    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    if (!dir) return CARD_RESULT_FATAL_ERROR;

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

static inline u16 fat_get(const u8* fat, u16 idx) { return rd16be(fat + (u32)idx * 2u); }
static inline void fat_set(u8* fat, u16 idx, u16 v) { wr16be(fat + (u32)idx * 2u, v); }

s32 __CARDUpdateFatBlock(s32 chan, u16* fat_u16, CARDCallback callback)
{
    u8* fat = (u8*)fat_u16;
    if (!fat) return CARD_RESULT_FATAL_ERROR;
    u16 cc = fat_get(fat, (u16)PORT_CARD_FAT_CHECKCODE);
    cc = (u16)(cc + 1u);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKCODE, cc);

    u16 cs = 0, csi = 0;
    oracle_check_sum(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUM, cs);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUMINV, csi);

    if (callback) callback(chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

s32 __CARDAllocBlock(s32 chan, u32 cBlock, CARDCallback callback)
{
    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) return CARD_RESULT_NOCARD;

    u8* fat = (u8*)(uintptr_t)card->current_fat_ptr;
    if (!fat) return CARD_RESULT_FATAL_ERROR;

    u16 freeBlocks = fat_get(fat, (u16)PORT_CARD_FAT_FREEBLOCKS);
    if (freeBlocks < (u16)cBlock) return CARD_RESULT_INSSPACE;
    fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, (u16)(freeBlocks - (u16)cBlock));

    u16 startBlock = 0xFFFFu;
    u16 iBlock = fat_get(fat, (u16)PORT_CARD_FAT_LASTSLOT);
    u16 prev = 0u;
    u16 count = 0u;

    while (cBlock > 0u) {
        if ((u32)card->cblock - (u32)CARD_NUM_SYSTEM_BLOCK < (u32)(++count)) return CARD_RESULT_FATAL_ERROR;
        iBlock++;
        if (iBlock < (u16)CARD_NUM_SYSTEM_BLOCK || (u32)card->cblock <= (u32)iBlock) iBlock = (u16)CARD_NUM_SYSTEM_BLOCK;
        if (fat_get(fat, iBlock) == PORT_CARD_FAT_AVAIL) {
            if (startBlock == 0xFFFFu) startBlock = iBlock;
            else fat_set(fat, prev, iBlock);
            prev = iBlock;
            fat_set(fat, iBlock, 0xFFFFu);
            cBlock--;
        }
    }

    fat_set(fat, (u16)PORT_CARD_FAT_LASTSLOT, iBlock);
    card->startBlock = startBlock;

    return __CARDUpdateFatBlock(chan, (u16*)fat, callback);
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

static void CreateCallbackFat(s32 chan, s32 result)
{
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;

    if (result < 0) goto error;

    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    u8* ent = dir + (u32)card->freeNo * (u32)PORT_CARD_DIR_SIZE;

    memcpy(ent + PORT_CARD_DIR_OFF_GAMENAME, card->id, 4);
    memcpy(ent + PORT_CARD_DIR_OFF_COMPANY, card->id + 4, 2);
    ent[PORT_CARD_DIR_OFF_PERMISSION] = (u8)PORT_CARD_ATTR_PUBLIC;
    ent[PORT_CARD_DIR_OFF_COPY_TIMES] = 0;
    wr16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK, card->startBlock);

    ent[PORT_CARD_DIR_OFF_BANNER_FORMAT] = 0;
    wr32be(ent + PORT_CARD_DIR_OFF_ICON_ADDR, 0xFFFFFFFFu);
    wr16be(ent + PORT_CARD_DIR_OFF_ICON_FORMAT, 0);
    wr16be(ent + PORT_CARD_DIR_OFF_ICON_SPEED, 1u);
    wr32be(ent + PORT_CARD_DIR_OFF_COMMENT_ADDR, 0xFFFFFFFFu);

    CARDFileInfo* fi = (CARDFileInfo*)(uintptr_t)card->fileInfo;
    fi->offset = 0;
    fi->iBlock = card->startBlock;

    wr32be(ent + PORT_CARD_DIR_OFF_TIME, oracle_time_seconds());

    result = __CARDUpdateDir(chan, callback);
    if (result < 0) goto error;
    return;

error:
    __CARDPutControlBlock(card, result);
    if (callback) callback(chan, result);
}

s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, CARDCallback callback)
{
    if (!fileName) return CARD_RESULT_FATAL_ERROR;
    if (strlen(fileName) > (u32)PORT_CARD_FILENAME_MAX) return CARD_RESULT_NAMETOOLONG;

    GcCardControl* card;
    s32 result = __CARDGetControlBlock(chan, &card);
    if (result < 0) return result;

    if (size == 0u || (size % (u32)card->sector_size) != 0u) {
        return __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
    }

    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    u16 freeNo = 0xFFFFu;
    for (u16 fileNo = 0u; fileNo < (u16)PORT_CARD_MAX_FILE; fileNo++) {
        const u8* ent = dir + (u32)fileNo * (u32)PORT_CARD_DIR_SIZE;
        if (ent[PORT_CARD_DIR_OFF_GAMENAME] == 0xFFu) {
            if (freeNo == 0xFFFFu) freeNo = fileNo;
            continue;
        }
        if (memcmp(ent + PORT_CARD_DIR_OFF_GAMENAME, card->id, 4) == 0 &&
            memcmp(ent + PORT_CARD_DIR_OFF_COMPANY, card->id + 4, 2) == 0 &&
            card_compare_filename(ent, fileName)) {
            return __CARDPutControlBlock(card, CARD_RESULT_EXIST);
        }
    }
    if (freeNo == 0xFFFFu) return __CARDPutControlBlock(card, CARD_RESULT_NOENT);

    u8* fat = (u8*)(uintptr_t)card->current_fat_ptr;
    u16 freeBlocks = fat_get(fat, (u16)PORT_CARD_FAT_FREEBLOCKS);
    if ((u32)card->sector_size * (u32)freeBlocks < size) {
        return __CARDPutControlBlock(card, CARD_RESULT_INSSPACE);
    }

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    card->freeNo = freeNo;

    u8* ent = dir + (u32)freeNo * (u32)PORT_CARD_DIR_SIZE;
    wr16be(ent + PORT_CARD_DIR_OFF_LENGTH, (u16)(size / (u32)card->sector_size));
    memset(ent + PORT_CARD_DIR_OFF_FILENAME, 0, PORT_CARD_FILENAME_MAX);
    for (u32 i = 0; i < (u32)PORT_CARD_FILENAME_MAX; i++) {
        ent[PORT_CARD_DIR_OFF_FILENAME + i] = (u8)fileName[i];
        if (fileName[i] == '\0') break;
    }

    card->fileInfo = (uintptr_t)fileInfo;
    fileInfo->chan = chan;
    fileInfo->fileNo = (s32)freeNo;

    result = __CARDAllocBlock(chan, size / (u32)card->sector_size, CreateCallbackFat);
    if (result < 0) return __CARDPutControlBlock(card, result);
    return result;
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

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result)
{
    (void)chan;
    g_cb_calls++;
    g_cb_last = result;
}

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
    oracle_check_sum(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUM, cs);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUMINV, csi);
}

void oracle_card_create_pbt_001_suite(u8* out)
{
    // Memory layout in GC RAM.
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

    gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
    gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

    // Seed one existing entry that should collide on name.
    u8* ent0 = dir + 0u * (u32)PORT_CARD_DIR_SIZE;
    memcpy(ent0 + PORT_CARD_DIR_OFF_GAMENAME, gc_card_block[0].id, 4);
    memcpy(ent0 + PORT_CARD_DIR_OFF_COMPANY, gc_card_block[0].id + 4, 2);
    memset(ent0 + PORT_CARD_DIR_OFF_FILENAME, 0, PORT_CARD_FILENAME_MAX);
    memcpy(ent0 + PORT_CARD_DIR_OFF_FILENAME, "dup", 4);

    u32 w = 0;
    wr32be(out + w, 0x43435230u); w += 4; // CCR0

    // Case A: success.
    CARDFileInfo fi;
    memset(&fi, 0xE5u, sizeof(fi));
    g_cb_calls = 0;
    g_cb_last = 0;
    s32 rc = CARDCreateAsync(0, "ok", 2u * 8192u, &fi, cb_record);

    wr32be(out + w, 0x43435231u); w += 4; // CCR1
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, (u32)gc_card_block[0].result); w += 4;
    wr32be(out + w, (u32)gc_card_block[0].freeNo); w += 4;
    wr32be(out + w, (u32)gc_card_block[0].startBlock); w += 4;
    wr32be(out + w, (u32)fi.fileNo); w += 4;
    wr32be(out + w, (u32)fi.offset); w += 4;
    wr32be(out + w, (u32)fi.iBlock); w += 4;
    wr32be(out + w, fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE)); w += 4;

    // Case B: duplicate.
    memset(&fi, 0xE5u, sizeof(fi));
    g_cb_calls = 0;
    g_cb_last = 0;
    u32 h_dir_before = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);
    u32 h_fat_before = fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE);
    rc = CARDCreateAsync(0, "dup", 1u * 8192u, &fi, cb_record);

    wr32be(out + w, 0x43435232u); w += 4; // CCR2
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;
    wr32be(out + w, (u32)g_cb_last); w += 4;
    wr32be(out + w, fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, fnv1a32(fat, CARD_SYSTEM_BLOCK_SIZE)); w += 4;
    wr32be(out + w, h_dir_before); w += 4;
    wr32be(out + w, h_fat_before); w += 4;

    // Case C: insufficient space.
    // Force freeBlocks to 0.
    fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, 0);
    g_cb_calls = 0;
    rc = CARDCreateAsync(0, "ns", 1u * 8192u, &fi, cb_record);
    wr32be(out + w, 0x43435233u); w += 4; // CCR3
    wr32be(out + w, (u32)rc); w += 4;
    wr32be(out + w, (u32)g_cb_calls); w += 4;

    // Case D: name too long.
    rc = CARDCreateAsync(0, "0123456789012345678901234567890123", 1u * 8192u, &fi, cb_record);
    wr32be(out + w, 0x43435234u); w += 4; // CCR4
    wr32be(out + w, (u32)rc); w += 4;
}
