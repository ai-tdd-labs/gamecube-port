#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BUSY = -1,
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_FATAL_ERROR = -128,
};

enum {
    PORT_CARD_MAX_FILE = 127,
    PORT_CARD_DIR_SIZE = 64,
    PORT_CARD_DIR_OFF_FILENAME = 8,
    PORT_CARD_FAT_FREEBLOCKS = 3,
};

typedef struct {
    s32 result;
    u32 thread_queue_cookie;
    s32 attached;
    u16 cBlock;
    s32 sector_size;
    u32 disk_id;
    u32 current_dir_ptr;
    u32 current_fat_ptr;
    u32 api_callback;
} GcCardControl;

GcCardControl gc_card_block[2];

int OSDisableInterrupts(void) { return 1; }
void OSRestoreInterrupts(int level) { (void)level; }

s32 __CARDGetControlBlock(s32 chan, GcCardControl **pcard)
{
    int enabled;
    s32 result;
    GcCardControl *card;

    if (chan < 0 || chan >= 2) {
        return CARD_RESULT_FATAL_ERROR;
    }

    card = &gc_card_block[chan];
    if (!card->disk_id) {
        return CARD_RESULT_FATAL_ERROR;
    }

    enabled = OSDisableInterrupts();
    if (!card->attached) {
        result = CARD_RESULT_NOCARD;
    } else if (card->result == CARD_RESULT_BUSY) {
        result = CARD_RESULT_BUSY;
    } else {
        card->result = CARD_RESULT_BUSY;
        card->api_callback = 0;
        if (pcard) {
            *pcard = card;
        }
        result = CARD_RESULT_READY;
    }
    OSRestoreInterrupts(enabled);
    return result;
}

s32 __CARDPutControlBlock(GcCardControl *card, s32 result)
{
    int enabled = OSDisableInterrupts();
    if (card->attached || card->result == CARD_RESULT_BUSY) {
        card->result = result;
    }
    OSRestoreInterrupts(enabled);
    return result;
}

static inline u16 rd16be(const u8 *p)
{
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

s32 CARDFreeBlocks(s32 chan, s32 *byteNotUsed, s32 *filesNotUsed)
{
    GcCardControl *card;
    s32 result;
    s32 fileNo;
    const u8 *dir;
    const u8 *fat;

    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    dir = (const u8 *)card->current_dir_ptr;
    fat = (const u8 *)card->current_fat_ptr;
    if (!dir || !fat) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }

    if (byteNotUsed) {
        u16 freeBlocks = rd16be(fat + PORT_CARD_FAT_FREEBLOCKS * 2u);
        *byteNotUsed = (s32)(card->sector_size * freeBlocks);
    }

    if (filesNotUsed) {
        *filesNotUsed = 0;
        for (fileNo = 0; fileNo < PORT_CARD_MAX_FILE; fileNo++) {
            if (dir[(u32)fileNo * PORT_CARD_DIR_SIZE + PORT_CARD_DIR_OFF_FILENAME] == 0xFFu) {
                ++*filesNotUsed;
            }
        }
    }

    return __CARDPutControlBlock(card, CARD_RESULT_READY);
}

s32 CARDGetSectorSize(s32 chan, u32 *size)
{
    GcCardControl *card;
    s32 result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }
    if (size) {
        *size = (u32)card->sector_size;
    }
    return __CARDPutControlBlock(card, CARD_RESULT_READY);
}

static inline void wr32be(volatile u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static inline void wr16be(u8 *p, u16 v)
{
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static inline void write_row(volatile u8 *out, u32 *off, u32 tag, s32 rc, s32 byteNotUsed, s32 filesNotUsed)
{
    wr32be(out + (*off) * 4u, tag);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)rc);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)byteNotUsed);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)filesNotUsed);
    (*off)++;
}

static inline void write_row_u32(volatile u8 *out, u32 *off, u32 tag, s32 rc, u32 sectorSize)
{
    wr32be(out + (*off) * 4u, tag);
    (*off)++;
    wr32be(out + (*off) * 4u, (u32)rc);
    (*off)++;
    wr32be(out + (*off) * 4u, sectorSize);
    (*off)++;
}

static void reset_state(void)
{
    for (u32 i = 0; i < 2; i++) {
        gc_card_block[i].result = CARD_RESULT_READY;
        gc_card_block[i].attached = 0;
        gc_card_block[i].current_dir_ptr = 0;
        gc_card_block[i].current_fat_ptr = 0;
        gc_card_block[i].sector_size = 8192;
        gc_card_block[i].disk_id = 0x80000000u;
    }
    gc_card_block[0].cBlock = 64;
}

static u8 dir_block[PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE];
static u16 fat_block[512];

int main(void)
{
    volatile u8 *out = (volatile u8 *)0x80300000u;
    for (u32 i = 0; i < 0x100u; i++) {
        out[i] = 0;
    }

    for (u32 i = 0; i < sizeof(dir_block); i++) {
        dir_block[i] = 0xFFu;
    }
    for (u32 i = 0; i < 512; i++) {
        fat_block[i] = 0;
    }
    dir_block[0 * PORT_CARD_DIR_SIZE + PORT_CARD_DIR_OFF_FILENAME] = 0x00u;
    dir_block[1 * PORT_CARD_DIR_SIZE + PORT_CARD_DIR_OFF_FILENAME] = 0x00u;

    wr16be((u8 *)fat_block + PORT_CARD_FAT_FREEBLOCKS * 2u, 7u);

    u32 off = 0u;
    s32 rc;
    s32 byteNotUsed = -1;
    s32 filesNotUsed = -1;
    u32 sectorSize = 0xDEADBEEFu;

    // L0: invalid channel -> fatal.
    reset_state();
    byteNotUsed = -1;
    filesNotUsed = -1;
    rc = CARDFreeBlocks(-1, &byteNotUsed, &filesNotUsed);
    write_row(out, &off, 0x43465241u, rc, byteNotUsed, filesNotUsed);

    // L1: busy -> CARD_RESULT_BUSY.
    reset_state();
    gc_card_block[0].attached = 1;
    gc_card_block[0].current_dir_ptr = (u32)(uintptr_t)dir_block;
    gc_card_block[0].current_fat_ptr = (u32)(uintptr_t)fat_block;
    gc_card_block[0].result = CARD_RESULT_BUSY;
    byteNotUsed = -1;
    filesNotUsed = -1;
    rc = CARDFreeBlocks(0, &byteNotUsed, &filesNotUsed);
    write_row(out, &off, 0x43465242u, rc, byteNotUsed, filesNotUsed);

    // L2: missing pointers -> BROKEN.
    reset_state();
    gc_card_block[0].attached = 1;
    byteNotUsed = -1;
    filesNotUsed = -1;
    rc = CARDFreeBlocks(0, &byteNotUsed, &filesNotUsed);
    write_row(out, &off, 0x43465243u, rc, byteNotUsed, filesNotUsed);

    // L3: valid count/read.
    reset_state();
    gc_card_block[0].attached = 1;
    gc_card_block[0].current_dir_ptr = (u32)(uintptr_t)dir_block;
    gc_card_block[0].current_fat_ptr = (u32)(uintptr_t)fat_block;
    gc_card_block[0].sector_size = 8192;
    byteNotUsed = -1;
    filesNotUsed = -1;
    rc = CARDFreeBlocks(0, &byteNotUsed, &filesNotUsed);
    write_row(out, &off, 0x43465244u, rc, byteNotUsed, filesNotUsed);

    // L4: CARDGetSectorSize invalid channel.
    reset_state();
    sectorSize = 0xDEADBEEFu;
    rc = CARDGetSectorSize(-1, &sectorSize);
    write_row_u32(out, &off, 0x43465331u, rc, sectorSize);

    // L5: CARDGetSectorSize valid.
    reset_state();
    gc_card_block[0].attached = 1;
    gc_card_block[0].current_dir_ptr = (u32)(uintptr_t)dir_block;
    gc_card_block[0].current_fat_ptr = (u32)(uintptr_t)fat_block;
    gc_card_block[0].sector_size = 16384;
    sectorSize = 0xFFFFFFFFu;
    rc = CARDGetSectorSize(0, &sectorSize);
    write_row_u32(out, &off, 0x43465332u, rc, sectorSize);

    // L6: CARDGetSectorSize busy.
    reset_state();
    gc_card_block[0].attached = 1;
    gc_card_block[0].result = CARD_RESULT_BUSY;
    gc_card_block[0].current_dir_ptr = (u32)(uintptr_t)dir_block;
    gc_card_block[0].current_fat_ptr = (u32)(uintptr_t)fat_block;
    sectorSize = 0x12345678u;
    rc = CARDGetSectorSize(0, &sectorSize);
    write_row_u32(out, &off, 0x43465333u, rc, sectorSize);

    wr32be(out + 0x00u, 0x43464253u); // "CFBS"
    wr32be(out + 0x04u, off);

    while (1) {}
    return 0;
}
