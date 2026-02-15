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
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_NOPERM = -10,
    CARD_RESULT_LIMIT = -11,
    CARD_RESULT_FATAL_ERROR = -128,
    CARD_RESULT_CANCELED = -14,
};

enum { CARD_SEG_SIZE = 512u };
enum { CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u };
enum { CARD_NUM_SYSTEM_BLOCK = 5u };

GcCardControl gc_card_block[GC_CARD_CHANS];

static inline void wr16be(u8* p, u16 v) { p[0] = (u8)(v >> 8u); p[1] = (u8)v; }
static inline void wr32be(u8* p, u32 v) {
    p[0] = (u8)(v >> 24u);
    p[1] = (u8)(v >> 16u);
    p[2] = (u8)(v >> 8u);
    p[3] = (u8)v;
}
static inline u16 rd16be(const u8* p) { return (u16)(((u16)p[0] << 8u) | (u16)p[1]); }

static u32 fnv1a32(const u8* p, u32 n) {
    u32 h = 2166136261u;
    for (u32 i = 0; i < n; i++) { h ^= (u32)p[i]; h *= 16777619u; }
    return h;
}

static inline u32 trunc_to(u32 n, u32 a) { return n & ~(a - 1u); }
static inline u32 offset_in(u32 n, u32 a) { return n & (a - 1u); }

static inline u16 fat_get(const u8* fat, u16 idx) { return rd16be(fat + (u32)idx * 2u); }
static inline void fat_set(u8* fat, u16 idx, u16 v) { wr16be(fat + (u32)idx * 2u, v); }

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

s32 __CARDIsPublic(const void* dirEntry) {
    const u8* ent = (const u8*)dirEntry;
    if (!ent) return CARD_RESULT_FATAL_ERROR;
    if (ent[PORT_CARD_DIR_OFF_GAMENAME] == 0xFFu) return CARD_RESULT_NOFILE;
    if ((ent[PORT_CARD_DIR_OFF_PERMISSION] & (u8)PORT_CARD_ATTR_PUBLIC) != 0) return CARD_RESULT_READY;
    return CARD_RESULT_NOPERM;
}

static int card_is_valid_block_no(const GcCardControl* card, u16 block) {
    return ((u16)CARD_NUM_SYSTEM_BLOCK <= block && (u32)block < (u32)card->cblock);
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

    while (trunc_to((u32)fileInfo->offset, (u32)card->sector_size) <
           trunc_to((u32)offset, (u32)card->sector_size)) {
        fileInfo->offset += (s32)card->sector_size;
        fileInfo->iBlock = fat_get(fat, fileInfo->iBlock);
        if (!card_is_valid_block_no(card, fileInfo->iBlock)) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
    }

    fileInfo->offset = offset;
    if (pcard) *pcard = card;
    return CARD_RESULT_READY;
}

static u8 s_card_img[0x40000];

s32 __CARDRead(s32 chan, u32 addr, s32 length, void* dst, CARDCallback callback) {
    if (chan < 0 || chan >= GC_CARD_CHANS) return CARD_RESULT_FATAL_ERROR;
    GcCardControl* card = &gc_card_block[chan];
    if (!card->attached) return CARD_RESULT_NOCARD;
    if (!dst || length < 0) return CARD_RESULT_FATAL_ERROR;
    u32 len = (u32)length;
    if (addr > (u32)sizeof(s_card_img) || len > (u32)sizeof(s_card_img) - addr) return CARD_RESULT_FATAL_ERROR;
    memcpy(dst, &s_card_img[addr], len);
    card->buffer = (uintptr_t)((u8*)dst + len);
    if (callback) callback(chan, CARD_RESULT_READY);
    return CARD_RESULT_READY;
}

static void ReadCallback(s32 chan, s32 result) {
    GcCardControl* card = &gc_card_block[chan];
    CARDCallback callback;
    CARDFileInfo* fileInfo;
    s32 len_to_boundary;

    if (result < 0) goto error;

    fileInfo = (CARDFileInfo*)(uintptr_t)card->fileInfo;
    if (fileInfo->length < 0) { result = CARD_RESULT_CANCELED; goto error; }

    len_to_boundary =
        (s32)trunc_to((u32)fileInfo->offset + (u32)card->sector_size, (u32)card->sector_size) - fileInfo->offset;
    fileInfo->length -= len_to_boundary;
    if (fileInfo->length <= 0) goto error;

    const u8* fat = (const u8*)(uintptr_t)card->current_fat_ptr;
    if (!fat) { result = CARD_RESULT_BROKEN; goto error; }
    fileInfo->offset += len_to_boundary;
    fileInfo->iBlock = fat_get(fat, fileInfo->iBlock);
    if (!card_is_valid_block_no(card, fileInfo->iBlock)) { result = CARD_RESULT_BROKEN; goto error; }

    result = __CARDRead(chan,
                        card->sector_size * (u32)fileInfo->iBlock,
                        (fileInfo->length < (s32)card->sector_size) ? fileInfo->length : (s32)card->sector_size,
                        (void*)(uintptr_t)card->buffer,
                        ReadCallback);
    if (result < 0) goto error;
    return;

error:
    callback = (CARDCallback)(uintptr_t)card->api_callback;
    card->api_callback = 0u;
    __CARDPutControlBlock(card, result);
    if (callback) callback(chan, result);
}

s32 CARDReadAsync(CARDFileInfo* fileInfo, void* buf, s32 length, s32 offset, CARDCallback callback) {
    GcCardControl* card;
    s32 result;

    if (offset_in((u32)offset, CARD_SEG_SIZE) != 0u || offset_in((u32)length, CARD_SEG_SIZE) != 0u) {
        return CARD_RESULT_FATAL_ERROR;
    }

    result = __CARDSeek(fileInfo, length, offset, &card);
    if (result < 0) return result;

    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    u8* ent = dir + (u32)fileInfo->fileNo * (u32)PORT_CARD_DIR_SIZE;
    result = __CARDAccess(card, ent);
    if (result == CARD_RESULT_NOPERM) {
        result = __CARDIsPublic(ent);
    }
    if (result < 0) return __CARDPutControlBlock(card, result);

    card->api_callback = (uintptr_t)(callback ? callback : __CARDDefaultApiCallback);

    s32 off_in_sector = (s32)offset_in((u32)fileInfo->offset, (u32)card->sector_size);
    s32 first_len = (length < (s32)card->sector_size - off_in_sector) ? length : (s32)card->sector_size - off_in_sector;
    result = __CARDRead(fileInfo->chan,
                        card->sector_size * (u32)fileInfo->iBlock + (u32)off_in_sector,
                        first_len,
                        buf,
                        ReadCallback);
    if (result < 0) __CARDPutControlBlock(card, result);
    return result;
}

static s32 g_cb_calls;
static s32 g_cb_last;
static void cb_record(s32 chan, s32 result) { (void)chan; g_cb_calls++; g_cb_last = result; }

static void init_dir(u8* dir) {
    memset(dir, 0xFFu, CARD_SYSTEM_BLOCK_SIZE);
}

static void init_fat(u8* fat) {
    memset(fat, 0x00u, CARD_SYSTEM_BLOCK_SIZE);
    for (u16 b = (u16)CARD_NUM_SYSTEM_BLOCK; b < 64u; b++) fat_set(fat, b, PORT_CARD_FAT_AVAIL);
}

static void seed_entry(u8* dir, const u8* id6, u8 perm, u16 startBlock, u16 lenBlocks) {
    u8* ent = dir + 0u * (u32)PORT_CARD_DIR_SIZE;
    memcpy(ent + PORT_CARD_DIR_OFF_GAMENAME, id6, 4);
    memcpy(ent + PORT_CARD_DIR_OFF_COMPANY, id6 + 4, 2);
    ent[PORT_CARD_DIR_OFF_PERMISSION] = perm;
    wr16be(ent + PORT_CARD_DIR_OFF_STARTBLOCK, startBlock);
    wr16be(ent + PORT_CARD_DIR_OFF_LENGTH, lenBlocks);
}

static void init_img(void) {
    for (u32 i = 0; i < (u32)sizeof(s_card_img); i++) s_card_img[i] = (u8)(i ^ 0xA5u);
}

void oracle_card_read_api_pbt_001_suite(u8* out) {
    memset(out, 0, 0x200u);
    u32 w = 0;
    wr32be(out + w, 0x43525230u); w += 4; // CRR0

    // Common card state template.
    memset(gc_card_block, 0, sizeof(gc_card_block));
    gc_card_block[0].disk_id = 0x80000000u;
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_READY;
    gc_card_block[0].sector_size = 8192;
    gc_card_block[0].cblock = 64;
    memcpy(gc_card_block[0].id, "GAMEC0", 6);

    // Case A: read 1024 @ offset 0.
    {
        init_img();
        u8* dir = (u8*)0x80400000u;
        u8* fat = (u8*)0x80402000u;
        init_dir(dir);
        init_fat(fat);
        seed_entry(dir, (const u8*)"GAMEC0", (u8)PORT_CARD_ATTR_PUBLIC, 5, 2);
        fat_set(fat, 5, 6);
        fat_set(fat, 6, 0xFFFFu);

        gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
        gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

        u8* buf = (u8*)0x80404000u;
        memset(buf, 0xEEu, 1024u);

        CARDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.chan = 0;
        fi.fileNo = 0;
        fi.offset = 0;
        fi.iBlock = 5;

        g_cb_calls = 0;
        g_cb_last = 0;
        s32 rc = CARDReadAsync(&fi, buf, 1024, 0, cb_record);
        u32 h = fnv1a32(buf, 1024u);

        wr32be(out + w, 0x43525231u); w += 4;
        wr32be(out + w, (u32)rc); w += 4;
        wr32be(out + w, (u32)g_cb_calls); w += 4;
        wr32be(out + w, (u32)g_cb_last); w += 4;
        wr32be(out + w, h); w += 4;
    }

    // Case B: read 1024 @ offset 7680 (crosses sector boundary).
    {
        init_img();
        u8* dir = (u8*)0x80408000u;
        u8* fat = (u8*)0x8040A000u;
        init_dir(dir);
        init_fat(fat);
        seed_entry(dir, (const u8*)"GAMEC0", (u8)PORT_CARD_ATTR_PUBLIC, 5, 2);
        fat_set(fat, 5, 6);
        fat_set(fat, 6, 0xFFFFu);

        gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
        gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

        u8* buf = (u8*)0x8040C000u;
        memset(buf, 0xDDu, 1024u);

        CARDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.chan = 0;
        fi.fileNo = 0;
        fi.offset = 0;
        fi.iBlock = 5;

        g_cb_calls = 0;
        g_cb_last = 0;
        s32 rc = CARDReadAsync(&fi, buf, 1024, 7680, cb_record);
        u32 h = fnv1a32(buf, 1024u);

        wr32be(out + w, 0x43525232u); w += 4;
        wr32be(out + w, (u32)rc); w += 4;
        wr32be(out + w, (u32)g_cb_calls); w += 4;
        wr32be(out + w, (u32)g_cb_last); w += 4;
        wr32be(out + w, h); w += 4;
    }

    // Case C: misaligned length => fatal error, no callback.
    {
        init_img();
        u8* dir = (u8*)0x80410000u;
        u8* fat = (u8*)0x80412000u;
        init_dir(dir);
        init_fat(fat);
        seed_entry(dir, (const u8*)"GAMEC0", (u8)PORT_CARD_ATTR_PUBLIC, 5, 2);
        fat_set(fat, 5, 6);
        fat_set(fat, 6, 0xFFFFu);

        gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
        gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

        u8* buf = (u8*)0x80414000u;
        memset(buf, 0x11u, 512u);
        u32 h_before = fnv1a32(buf, 512u);

        CARDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.chan = 0;
        fi.fileNo = 0;
        fi.offset = 0;
        fi.iBlock = 5;

        g_cb_calls = 0;
        s32 rc = CARDReadAsync(&fi, buf, 1, 0, cb_record);
        u32 h_after = fnv1a32(buf, 512u);

        wr32be(out + w, 0x43525233u); w += 4;
        wr32be(out + w, (u32)rc); w += 4;
        wr32be(out + w, (u32)g_cb_calls); w += 4;
        wr32be(out + w, h_before); w += 4;
        wr32be(out + w, h_after); w += 4;
    }

    // Case D: NOPERM but PUBLIC => read allowed.
    {
        init_img();
        u8* dir = (u8*)0x80418000u;
        u8* fat = (u8*)0x8041A000u;
        init_dir(dir);
        init_fat(fat);
        seed_entry(dir, (const u8*)"BAD000", (u8)PORT_CARD_ATTR_PUBLIC, 5, 2);
        fat_set(fat, 5, 6);
        fat_set(fat, 6, 0xFFFFu);

        gc_card_block[0].current_dir_ptr = (uintptr_t)dir;
        gc_card_block[0].current_fat_ptr = (uintptr_t)fat;

        u8* buf = (u8*)0x8041C000u;
        memset(buf, 0x22u, 1024u);

        CARDFileInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.chan = 0;
        fi.fileNo = 0;
        fi.offset = 0;
        fi.iBlock = 5;

        g_cb_calls = 0;
        g_cb_last = 0;
        s32 rc = CARDReadAsync(&fi, buf, 1024, 0, cb_record);
        u32 h = fnv1a32(buf, 1024u);

        wr32be(out + w, 0x43525234u); w += 4;
        wr32be(out + w, (u32)rc); w += 4;
        wr32be(out + w, (u32)g_cb_calls); w += 4;
        wr32be(out + w, (u32)g_cb_last); w += 4;
        wr32be(out + w, h); w += 4;
    }
}

