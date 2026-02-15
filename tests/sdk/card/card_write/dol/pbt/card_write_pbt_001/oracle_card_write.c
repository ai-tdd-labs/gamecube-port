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
    CARD_RESULT_NOFILE = -4,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_NOPERM = -10,
    CARD_RESULT_LIMIT = -11,
    CARD_RESULT_FATAL_ERROR = -128,
    CARD_RESULT_CANCELED = -14,
};

enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };
enum { CARD_NUM_SYSTEM_BLOCK = 5u };
enum { TICKS_PER_SEC = (162000000u / 4u) };

GcCardControl gc_card_block[GC_CARD_CHANS];

static inline void wr16be(u8* p, u16 v) { p[0] = (u8)(v >> 8u); p[1] = (u8)v; }
static inline void wr32be(u8* p, u32 v) {
    p[0] = (u8)(v >> 24u);
    p[1] = (u8)(v >> 16u);
    p[2] = (u8)(v >> 8u);
    p[3] = (u8)v;
}
static inline u16 rd16be(const u8* p) { return (u16)(((u16)p[0] << 8u) | (u16)p[1]); }
static inline u32 rd32be(const u8* p) {
    return ((u32)p[0] << 24u) | ((u32)p[1] << 16u) | ((u32)p[2] << 8u) | (u32)p[3];
}

static u32 fnv1a32(const u8* p, u32 n) {
    u32 h = 2166136261u;
    for (u32 i = 0; i < n; i++) { h ^= (u32)p[i]; h *= 16777619u; }
    return h;
}

static void card_checksum_u16be(const u8* ptr, u32 length, u16* checksum, u16* checksumInv) {
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

static u32 oracle_time_seconds(void) {
    static u32 ticks;
    ticks += TICKS_PER_SEC;
    return ticks / TICKS_PER_SEC;
}

void __CARDDefaultApiCallback(s32 chan, s32 result) { (void)chan; (void)result; }

s32 __CARDGetControlBlock(s32 chan, GcCardControl** pcard) {
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

s32 __CARDPutControlBlock(GcCardControl* card, s32 result) {
    if (!card) return result;
    if (card->attached || card->result == CARD_RESULT_BUSY) {
        card->result = result;
    }
    return result;
}

s32 __CARDAccess(const GcCardControl* card, const void* dirEntry) {
    const u8* ent = (const u8*)dirEntry;
    if (!card || !ent) return CARD_RESULT_FATAL_ERROR;
    if (ent[PORT_CARD_DIR_OFF_GAMENAME] == 0xFFu) return CARD_RESULT_NOFILE;
    if (memcmp(ent + PORT_CARD_DIR_OFF_GAMENAME, card->id, 4) == 0 &&
        memcmp(ent + PORT_CARD_DIR_OFF_COMPANY, card->id + 4, 2) == 0) {
        return CARD_RESULT_READY;
    }
    return CARD_RESULT_NOPERM;
}

static int card_is_valid_block_no(const GcCardControl* card, u16 block) {
    return ((u16)CARD_NUM_SYSTEM_BLOCK <= block && (u32)block < (u32)card->cblock);
}

static s32 __CARDUpdateDir(s32 chan, CARDCallback callback) {
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

s32 __CARDSeek(CARDFileInfo* fileInfo, s32 length, s32 offset, GcCardControl** pcard) {
    if (!fileInfo) return CARD_RESULT_FATAL_ERROR;
    GcCardControl* card;
    s32 result = __CARDGetControlBlock(fileInfo->chan, &card);
    if (result < 0) return result;

    if (!card_is_valid_block_no(card, fileInfo->iBlock) ||
        (u32)card->cblock * (u32)card->sector_size <= (u32)fileInfo->offset) {
        return __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
    }

    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    if (!dir) return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    const u8* ent = dir + (u32)fileInfo->fileNo * (u32)PORT_CARD_DIR_SIZE;
    u16 ent_length = rd16be(ent + PORT_CARD_DIR_OFF_LENGTH);
    u16 ent_start = rd16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK);
    if ((u32)ent_length * (u32)card->sector_size <= (u32)offset ||
        (u32)ent_length * (u32)card->sector_size < (u32)offset + (u32)length) {
        return __CARDPutControlBlock(card, CARD_RESULT_LIMIT);
    }

    card->fileInfo = (uintptr_t)fileInfo;
    fileInfo->length = length;

    if (offset < fileInfo->offset) {
        fileInfo->offset = 0;
        fileInfo->iBlock = ent_start;
        if (!card_is_valid_block_no(card, fileInfo->iBlock)) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
    }

    const u8* fat = (const u8*)(uintptr_t)card->current_fat_ptr;
    if (!fat) return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);

    while ((u32)fileInfo->offset < (u32)offset) {
        fileInfo->offset += card->sector_size;
        fileInfo->iBlock = fat_get(fat, fileInfo->iBlock);
        if (!card_is_valid_block_no(card, fileInfo->iBlock)) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
    }

    if (pcard) *pcard = card;
    return CARD_RESULT_READY;
}

// Oracle "card storage" (raw byte address space).
static u8 s_card_img[0x40000];

s32 __CARDEraseSector(s32 chan, u32 addr, CARDCallback callback) {
    (void)chan;
    (void)addr;
    if (callback) callback(chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

s32 __CARDWrite(s32 chan, u32 addr, s32 length, const void* src, CARDCallback callback) {
    (void)chan;
    if (!src || length < 0) return CARD_RESULT_FATAL_ERROR;
    u32 len = (u32)length;
    if (addr > (u32)sizeof(s_card_img) || len > (u32)sizeof(s_card_img) - addr) return CARD_RESULT_FATAL_ERROR;
    memcpy(&s_card_img[addr], src, len);
    if (callback) callback(chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

static void EraseCallback(s32 chan, s32 result);

static void WriteCallback(s32 chan, s32 result) {
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback;
    CARDFileInfo* fileInfo;

    if (result < 0) goto error;

    fileInfo = (CARDFileInfo*)(uintptr_t)card->fileInfo;
    if (fileInfo->length < 0) {
        result = CARD_RESULT_CANCELED;
        goto error;
    }

    fileInfo->length -= (s32)card->sector_size;
    if (fileInfo->length <= 0) {
        u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
        if (!dir) { result = CARD_RESULT_BROKEN; goto error; }
        u8* ent = dir + (u32)fileInfo->fileNo * (u32)PORT_CARD_DIR_SIZE;
        wr32be(ent + PORT_CARD_DIR_OFF_TIME, oracle_time_seconds());

        callback = (CARDCallback)(uintptr_t)card->api_callback;
        card->api_callback = 0u;
        result = __CARDUpdateDir(chan, callback);
    } else {
        const u8* fat = (const u8*)(uintptr_t)card->current_fat_ptr;
        if (!fat) { result = CARD_RESULT_BROKEN; goto error; }
        fileInfo->offset += (s32)card->sector_size;
        fileInfo->iBlock = fat_get(fat, fileInfo->iBlock);
        if (!card_is_valid_block_no(card, fileInfo->iBlock)) { result = CARD_RESULT_BROKEN; goto error; }
        result = __CARDEraseSector(chan, card->sector_size * (u32)fileInfo->iBlock, EraseCallback);
    }

    if (result < 0) goto error;
    return;

error:
    callback = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;
    __CARDPutControlBlock(card, result);
    if (callback) callback(chan, result);
}

static void EraseCallback(s32 chan, s32 result) {
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback;

    if (result < 0) goto error;

    CARDFileInfo* fileInfo = (CARDFileInfo*)(uintptr_t)card->fileInfo;
    result = __CARDWrite(chan, card->sector_size * (u32)fileInfo->iBlock, (s32)card->sector_size,
                         (const void*)(uintptr_t)card->buffer, WriteCallback);
    if (result < 0) goto error;
    return;

error:
    callback = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;
    __CARDPutControlBlock(card, result);
    if (callback) callback(chan, result);
}

s32 CARDWriteAsync(CARDFileInfo* fileInfo, const void* buf, s32 length, s32 offset, CARDCallback callback) {
    GcCardControl* card;
    s32 result = __CARDSeek(fileInfo, length, offset, &card);
    if (result < 0) return result;

    if (((u32)offset % (u32)card->sector_size) != 0u || ((u32)length % (u32)card->sector_size) != 0u) {
        return __CARDPutControlBlock(card, CARD_RESULT_FATAL_ERROR);
    }

    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    if (!dir) return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    u8* ent = dir + (u32)fileInfo->fileNo * (u32)PORT_CARD_DIR_SIZE;
    result = __CARDAccess(card, ent);
    if (result < 0) return __CARDPutControlBlock(card, result);

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);
    card->buffer = (uintptr_t)buf;
    result = __CARDEraseSector(fileInfo->chan, card->sector_size * (u32)fileInfo->iBlock, EraseCallback);
    if (result < 0) __CARDPutControlBlock(card, result);
    return result;
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result) { (void)chan; g_cb_calls++; g_cb_last = result; }

static void init_dir(u8* dir, u16 checkCode) {
    memset(dir, 0xFFu, CARD_SYSTEM_BLOCK_SIZE);
    u8* chk = dir + (u32)PORT_CARD_MAX_FILE * (u32)PORT_CARD_DIR_SIZE;
    wr16be(chk + (PORT_CARD_DIR_SIZE - 6u), checkCode);
    u16 cs = 0, csi = 0;
    card_checksum_u16be(dir, CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 4u), cs);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 2u), csi);
}

static void init_fat(u8* fat, u16 checkCode, u16 cBlock) {
    memset(fat, 0x00u, CARD_SYSTEM_BLOCK_SIZE);
    for (u16 b = (u16)CARD_NUM_SYSTEM_BLOCK; b < cBlock; b++) fat_set(fat, b, PORT_CARD_FAT_AVAIL);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKCODE, checkCode);
    fat_set(fat, (u16)PORT_CARD_FAT_FREEBLOCKS, (u16)(cBlock - (u16)CARD_NUM_SYSTEM_BLOCK));
    fat_set(fat, (u16)PORT_CARD_FAT_LASTSLOT, (u16)((u16)CARD_NUM_SYSTEM_BLOCK - 1u));
    u16 cs = 0, csi = 0;
    card_checksum_u16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, 0x1FFCu, &cs, &csi);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUM, cs);
    fat_set(fat, (u16)PORT_CARD_FAT_CHECKSUMINV, csi);
}

static void seed_entry(u8* dir, u32 fileNo, const u8* id6, const char* name, u16 startBlock, u16 lenBlocks) {
    u8* ent = dir + fileNo * (u32)PORT_CARD_DIR_SIZE;
    memcpy(ent + PORT_CARD_DIR_OFF_GAMENAME, id6, 4);
    memcpy(ent + PORT_CARD_DIR_OFF_COMPANY, id6 + 4, 2);
    memset(ent + PORT_CARD_DIR_OFF_FILENAME, 0, PORT_CARD_FILENAME_MAX);
    for (u32 i = 0; i < (u32)PORT_CARD_FILENAME_MAX; i++) {
        ent[PORT_CARD_DIR_OFF_FILENAME + i] = (u8)name[i];
        if (name[i] == '\0') break;
    }
    wr16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK, startBlock);
    wr16be(ent + PORT_CARD_DIR_OFF_LENGTH, lenBlocks);
    wr32be(ent + PORT_CARD_DIR_OFF_TIME, 0u);
}

static void init_img(void) {
    for (u32 i = 0; i < (u32)sizeof(s_card_img); i++) s_card_img[i] = (u8)(i ^ 0xA5u);
}

void oracle_card_write_pbt_001_suite(u8* out) {
    memset(out, 0, 0x200u);
    u32 w = 0;
    wr32be(out + w, 0x43575230u); w += 4; // CWR0

    // Case A: 1 sector write @0.
    {
        init_img();
        u8* dir = (u8*)0x80400000u;
        u8* fat = (u8*)0x80402000u;
        init_dir(dir, 0);
        init_fat(fat, 0, 64);
        seed_entry(dir, 0, (const u8*)"GAMEC0", "file", 5, 2);
        fat_set(fat, 5, 6);
        fat_set(fat, 6, 0xFFFFu);

        memset(gc_card_block, 0, sizeof(gc_card_block));
        gc_card_block[0].disk_id = 0x80000000u;
        gc_card_block[0].attached = 1;
        gc_card_block[0].result = CARD_RESULT_READY;
        gc_card_block[0].sector_size = 8192;
        gc_card_block[0].cblock = 64;
        memcpy(gc_card_block[0].id, "GAMEC0", 6);
        gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
        gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

        static u8 wrbuf[8192];
        for (u32 i = 0; i < (u32)sizeof(wrbuf); i++) wrbuf[i] = (u8)(0x5Au ^ (u8)i);

        CARDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.chan = 0;
        fi.fileNo = 0;
        fi.offset = 0;
        fi.iBlock = 5;

        g_cb_calls = 0;
        g_cb_last = 0;
        s32 rc = CARDWriteAsync(&fi, wrbuf, 8192, 0, cb_record);

        u32 h_sec0 = fnv1a32(&s_card_img[5u * 8192u], 8192u);
        u32 t = rd32be(dir + 0u * (u32)PORT_CARD_DIR_SIZE + PORT_CARD_DIR_OFF_TIME);
        u32 h_dir = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);

        wr32be(out + w, 0x43575231u); w += 4;
        wr32be(out + w, (u32)rc); w += 4;
        wr32be(out + w, (u32)g_cb_calls); w += 4;
        wr32be(out + w, (u32)g_cb_last); w += 4;
        wr32be(out + w, t); w += 4;
        wr32be(out + w, h_sec0); w += 4;
        wr32be(out + w, h_dir); w += 4;
    }

    // Case B: 2 sector write @0.
    {
        init_img();
        u8* dir = (u8*)0x80404000u;
        u8* fat = (u8*)0x80406000u;
        init_dir(dir, 0);
        init_fat(fat, 0, 64);
        seed_entry(dir, 0, (const u8*)"GAMEC0", "file", 5, 2);
        fat_set(fat, 5, 6);
        fat_set(fat, 6, 0xFFFFu);

        memset(gc_card_block, 0, sizeof(gc_card_block));
        gc_card_block[0].disk_id = 0x80000000u;
        gc_card_block[0].attached = 1;
        gc_card_block[0].result = CARD_RESULT_READY;
        gc_card_block[0].sector_size = 8192;
        gc_card_block[0].cblock = 64;
        memcpy(gc_card_block[0].id, "GAMEC0", 6);
        gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
        gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

        static u8 wrbuf[16384];
        for (u32 i = 0; i < (u32)sizeof(wrbuf); i++) wrbuf[i] = (u8)(0xA6u ^ (u8)i);

        CARDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.chan = 0;
        fi.fileNo = 0;
        fi.offset = 0;
        fi.iBlock = 5;

        g_cb_calls = 0;
        g_cb_last = 0;
        s32 rc = CARDWriteAsync(&fi, wrbuf, 16384, 0, cb_record);

        u32 h_sec0 = fnv1a32(&s_card_img[5u * 8192u], 8192u);
        u32 h_sec1 = fnv1a32(&s_card_img[6u * 8192u], 8192u);
        u32 t = rd32be(dir + 0u * (u32)PORT_CARD_DIR_SIZE + PORT_CARD_DIR_OFF_TIME);
        u32 h_dir = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);

        wr32be(out + w, 0x43575232u); w += 4;
        wr32be(out + w, (u32)rc); w += 4;
        wr32be(out + w, (u32)g_cb_calls); w += 4;
        wr32be(out + w, (u32)g_cb_last); w += 4;
        wr32be(out + w, t); w += 4;
        wr32be(out + w, h_sec0); w += 4;
        wr32be(out + w, h_sec1); w += 4;
        wr32be(out + w, h_dir); w += 4;
    }

    // Case C: misaligned length => fatal error.
    {
        init_img();
        u8* dir = (u8*)0x80408000u;
        u8* fat = (u8*)0x8040A000u;
        init_dir(dir, 0);
        init_fat(fat, 0, 64);
        seed_entry(dir, 0, (const u8*)"GAMEC0", "file", 5, 2);
        fat_set(fat, 5, 6);
        fat_set(fat, 6, 0xFFFFu);

        memset(gc_card_block, 0, sizeof(gc_card_block));
        gc_card_block[0].disk_id = 0x80000000u;
        gc_card_block[0].attached = 1;
        gc_card_block[0].result = CARD_RESULT_READY;
        gc_card_block[0].sector_size = 8192;
        gc_card_block[0].cblock = 64;
        memcpy(gc_card_block[0].id, "GAMEC0", 6);
        gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
        gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

        u8 wrbuf[8192];
        memset(wrbuf, 0x11, sizeof(wrbuf));
        CARDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.chan = 0;
        fi.fileNo = 0;
        fi.iBlock = 5;

        g_cb_calls = 0;
        u32 h_before = fnv1a32(&s_card_img[5u * 8192u], 8192u);
        s32 rc = CARDWriteAsync(&fi, wrbuf, 1, 0, cb_record);
        u32 h_after = fnv1a32(&s_card_img[5u * 8192u], 8192u);
        u32 h_dir = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);

        wr32be(out + w, 0x43575233u); w += 4;
        wr32be(out + w, (u32)rc); w += 4;
        wr32be(out + w, (u32)g_cb_calls); w += 4;
        wr32be(out + w, h_before); w += 4;
        wr32be(out + w, h_after); w += 4;
        wr32be(out + w, h_dir); w += 4;
    }

    // Case D: NOPERM (wrong game/company).
    {
        init_img();
        u8* dir = (u8*)0x8040C000u;
        u8* fat = (u8*)0x8040E000u;
        init_dir(dir, 0);
        init_fat(fat, 0, 64);
        seed_entry(dir, 0, (const u8*)"BAD000", "file", 5, 2);
        fat_set(fat, 5, 6);
        fat_set(fat, 6, 0xFFFFu);

        memset(gc_card_block, 0, sizeof(gc_card_block));
        gc_card_block[0].disk_id = 0x80000000u;
        gc_card_block[0].attached = 1;
        gc_card_block[0].result = CARD_RESULT_READY;
        gc_card_block[0].sector_size = 8192;
        gc_card_block[0].cblock = 64;
        memcpy(gc_card_block[0].id, "GAMEC0", 6);
        gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
        gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

        u8 wrbuf[8192];
        memset(wrbuf, 0x22, sizeof(wrbuf));
        CARDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.chan = 0;
        fi.fileNo = 0;
        fi.iBlock = 5;

        g_cb_calls = 0;
        u32 h_before = fnv1a32(&s_card_img[5u * 8192u], 8192u);
        s32 rc = CARDWriteAsync(&fi, wrbuf, 8192, 0, cb_record);
        u32 h_after = fnv1a32(&s_card_img[5u * 8192u], 8192u);
        u32 h_dir = fnv1a32(dir, CARD_SYSTEM_BLOCK_SIZE);

        wr32be(out + w, 0x43575234u); w += 4;
        wr32be(out + w, (u32)rc); w += 4;
        wr32be(out + w, (u32)g_cb_calls); w += 4;
        wr32be(out + w, h_before); w += 4;
        wr32be(out + w, h_after); w += 4;
        wr32be(out + w, h_dir); w += 4;
    }
}
