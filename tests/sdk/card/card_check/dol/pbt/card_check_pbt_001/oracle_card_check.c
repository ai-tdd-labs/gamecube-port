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
typedef int16_t s16;

typedef struct OSSramEx {
    u8 flashID[2][12];
    u32 wirelessKeyboardID;
    u16 wirelessPadID[4];
    u8 dvdErrorCode;
    u8 _padding0;
    u8 flashIDCheckSum[2];
    u16 gbs;
    u8 _padding1[2];
} OSSramEx;

typedef void (*CARDCallback)(s32 chan, s32 result);

enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_ENCODING = -13,
    CARD_RESULT_FATAL_ERROR = -128,
    CARD_RESULT_BUSY = -1,
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_NOFILE = -4,
    CARD_RESULT_NOPERM = -10,
};

enum {
    CARD_SYSTEM_BLOCK_SIZE = 8u * 1024u,
    CARD_NUM_SYSTEM_BLOCK = 5u,
};

static GcCardControl __CARDBlock[2];
static OSSramEx s_sram_ex;
static u8 s_sram_locked;
static u16 s_oracle_font_encode;
static s32 g_oracle_cb_calls;
static s32 g_oracle_cb_last;

static inline void wr16be(u8* p, u16 v)
{
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static inline u16 rd16be(const u8* p)
{
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static inline void wr32be(u8* p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static inline u64 rd64be(const u8* p)
{
    u64 v = 0;
    for (u32 i = 0; i < 8u; i++) {
        v = (v << 8u) | (u64)p[i];
    }
    return v;
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

void __CARDCheckSum(void* ptr, int length, u16* checksum, u16* checksumInv)
{
    card_checksum_u16be((const u8*)ptr, (u32)length, checksum, checksumInv);
}

void oracle_set_card_font_encode(u16 encode)
{
    s_oracle_font_encode = encode;
}

static u16 OSGetFontEncode(void)
{
    return s_oracle_font_encode;
}

static OSSramEx* __OSLockSramEx(void)
{
    if (s_sram_locked) {
        return 0;
    }
    s_sram_locked = 1u;
    return &s_sram_ex;
}

static s32 __OSUnlockSramEx(s32 commit)
{
    (void)commit;
    s_sram_locked = 0u;
    return 0;
}

s32 __CARDGetControlBlock(s32 chan, GcCardControl** pcard)
{
    if (chan < 0 || chan >= 2) {
        return CARD_RESULT_FATAL_ERROR;
    }

    GcCardControl* card = &__CARDBlock[chan];
    if (!card->disk_id) {
        return CARD_RESULT_FATAL_ERROR;
    }
    if (!card->attached) {
        return CARD_RESULT_NOCARD;
    }
    if (card->result == CARD_RESULT_BUSY) {
        return CARD_RESULT_BUSY;
    }

    card->result = CARD_RESULT_BUSY;
    card->api_callback = 0u;
    if (pcard) {
        *pcard = card;
    }
    return CARD_RESULT_READY;
}

s32 __CARDPutControlBlock(GcCardControl* card, s32 result)
{
    if (!card) {
        return result;
    }
    if (card->attached || card->result == CARD_RESULT_BUSY) {
        card->result = result;
    }
    return result;
}

static s32 __CARDUpdateDir(s32 chan, CARDCallback callback)
{
    GcCardControl* card = &__CARDBlock[chan];

    if (!card->current_dir_ptr) {
        return CARD_RESULT_BROKEN;
    }
    u8* dir = (u8*)(uintptr_t)card->current_dir_ptr;
    u8* chk = dir + (u32)PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE;
    s16 cc = (s16)rd16be(chk + (u32)PORT_CARD_DIR_SIZE - 6u);
    cc = (s16)(cc + 1);
    wr16be(chk + (u32)PORT_CARD_DIR_SIZE - 6u, (u16)cc);

    u16 cs = 0;
    u16 csi = 0;
    __CARDCheckSum(dir, (int)(CARD_SYSTEM_BLOCK_SIZE - 4u), &cs, &csi);
    wr16be(chk + (u32)PORT_CARD_DIR_SIZE - 4u, cs);
    wr16be(chk + (u32)PORT_CARD_DIR_SIZE - 2u, csi);

    __CARDPutControlBlock(card, CARD_RESULT_READY);
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    return CARD_RESULT_READY;
}

s32 __CARDUpdateFatBlock(s32 chan, u16* fat_u16, CARDCallback callback)
{
    if (!fat_u16) {
        return CARD_RESULT_FATAL_ERROR;
    }

    u8* fat = (u8*)fat_u16;
    u16 cc = rd16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u);
    cc = (u16)(cc + 1u);
    wr16be(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, cc);

    u16 cs = 0;
    u16 csi = 0;
    __CARDCheckSum(fat + (u32)PORT_CARD_FAT_CHECKCODE * 2u, CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
    wr16be(fat + (u32)PORT_CARD_FAT_CHECKSUM * 2u, cs);
    wr16be(fat + (u32)PORT_CARD_FAT_CHECKSUMINV * 2u, csi);

    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    return CARD_RESULT_READY;
}

static inline int card_is_valid_block_no(const GcCardControl* card, u16 iBlock)
{
    return ((u16)CARD_NUM_SYSTEM_BLOCK <= iBlock && (u32)iBlock < card->cblock);
}

static s32 VerifyID(GcCardControl* card)
{
    u8* id = (u8*)(uintptr_t)card->work_area;
    if (!id) {
        return CARD_RESULT_BROKEN;
    }

    u16 deviceID = rd16be(id + 32u);
    u16 size = rd16be(id + 34u);
    u16 encode = rd16be(id + 36u);
    if (deviceID != 0u || size != card->size_u16) {
        return CARD_RESULT_BROKEN;
    }

    u16 cs = 0;
    u16 csi = 0;
    __CARDCheckSum(id, 512u - 4u, &cs, &csi);
    if (rd16be(id + 508u) != cs || rd16be(id + 510u) != csi) {
        return CARD_RESULT_BROKEN;
    }

    if (encode != OSGetFontEncode()) {
        return CARD_RESULT_ENCODING;
    }

    u64 rand = rd64be(id + 12u);
    s32 chan = (s32)(card - __CARDBlock);
    OSSramEx* sram = __OSLockSramEx();
    if (!sram) {
        return CARD_RESULT_BROKEN;
    }

    for (u32 i = 0u; i < 12u; i++) {
        rand = (rand * 1103515245ull + 12345ull) >> 16;
        if (id[i] != (u8)(sram->flashID[chan][i] + (u8)rand)) {
            (void)__OSUnlockSramEx(0);
            return CARD_RESULT_BROKEN;
        }
        rand = ((rand * 1103515245ull + 12345ull) >> 16) & 0x7FFFull;
    }
    (void)__OSUnlockSramEx(0);
    return CARD_RESULT_READY;
}

static s32 VerifyDir(GcCardControl* card, s32* outCurrent)
{
    u8* wa = (u8*)(uintptr_t)card->work_area;
    u8* dir[2];
    u8* chk[2];
    u16 cs = 0;
    u16 csi = 0;
    s32 errors = 0;
    s32 current = 0;

    if (outCurrent) {
        *outCurrent = 0;
    }

    for (s32 i = 0; i < 2; i++) {
        dir[i] = wa + (u32)(1u + (u32)i) * CARD_SYSTEM_BLOCK_SIZE;
        chk[i] = dir[i] + (u32)PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE;
        __CARDCheckSum(dir[i], (int)(CARD_SYSTEM_BLOCK_SIZE - 4u), &cs, &csi);

        if (rd16be(chk[i] + (u32)PORT_CARD_DIR_SIZE - 4u) != cs ||
            rd16be(chk[i] + (u32)PORT_CARD_DIR_SIZE - 2u) != csi) {
            errors++;
            current = i;
            card->current_dir_ptr = 0u;
        }
    }

    if (errors == 0) {
        if (!card->current_dir_ptr) {
            s16 cc0 = (s16)rd16be(chk[0] + (u32)PORT_CARD_DIR_SIZE - 6u);
            s16 cc1 = (s16)rd16be(chk[1] + (u32)PORT_CARD_DIR_SIZE - 6u);
            current = (cc0 - cc1) < 0 ? 0 : 1;
            card->current_dir_ptr = (uintptr_t)dir[current];
            memcpy(dir[current], dir[current ^ 1u], CARD_SYSTEM_BLOCK_SIZE);
        }
        else {
            current = (card->current_dir_ptr == (uintptr_t)dir[0]) ? 0 : 1;
        }
    }

    if (outCurrent) {
        *outCurrent = current;
    }
    return errors;
}

static s32 VerifyFAT(GcCardControl* card, s32* outCurrent)
{
    u8* wa = (u8*)(uintptr_t)card->work_area;
    u8* fat[2];
    u16 cs = 0;
    u16 csi = 0;
    s32 errors = 0;
    s32 current = 0;

    if (outCurrent) {
        *outCurrent = 0;
    }

    for (s32 i = 0; i < 2; i++) {
        fat[i] = wa + (u32)(3u + (u32)i) * CARD_SYSTEM_BLOCK_SIZE;
        __CARDCheckSum(fat[i] + (u32)PORT_CARD_FAT_CHECKCODE * 2u, (int)(CARD_SYSTEM_BLOCK_SIZE - 4u), &cs, &csi);

        if (rd16be(fat[i] + (u32)PORT_CARD_FAT_CHECKSUM * 2u) != cs ||
            rd16be(fat[i] + (u32)PORT_CARD_FAT_CHECKSUMINV * 2u) != csi) {
            errors++;
            current = i;
            card->current_fat_ptr = 0u;
            continue;
        }

        u16 freeBlocks = 0;
        for (u16 iBlock = CARD_NUM_SYSTEM_BLOCK; iBlock < (u16)card->cblock; iBlock++) {
            if (rd16be(fat[i] + (u32)iBlock * 2u) == PORT_CARD_FAT_AVAIL) {
                freeBlocks++;
            }
        }
        if (freeBlocks != rd16be(fat[i] + (u32)PORT_CARD_FAT_FREEBLOCKS * 2u)) {
            errors++;
            current = i;
            card->current_fat_ptr = 0u;
        }
    }

    if (errors == 0) {
        if (!card->current_fat_ptr) {
            s16 cc0 = (s16)rd16be(fat[0] + (u32)PORT_CARD_FAT_CHECKCODE * 2u);
            s16 cc1 = (s16)rd16be(fat[1] + (u32)PORT_CARD_FAT_CHECKCODE * 2u);
            current = (cc0 - cc1) < 0 ? 0 : 1;
            card->current_fat_ptr = (uintptr_t)fat[current];
            memcpy(fat[current], fat[current ^ 1u], CARD_SYSTEM_BLOCK_SIZE);
        }
        else {
            current = (card->current_fat_ptr == (uintptr_t)fat[0]) ? 0 : 1;
        }
    }

    if (outCurrent) {
        *outCurrent = current;
    }
    return errors;
}

static s32 __CARDVerify(GcCardControl* card)
{
    s32 result = VerifyID(card);
    s32 errors = 0;

    if (result < 0) {
        return result;
    }

    errors = VerifyDir(card, 0);
    errors += VerifyFAT(card, 0);
    return errors == 0 ? CARD_RESULT_READY : CARD_RESULT_BROKEN;
}

s32 CARDCheckExAsync(s32 chan, s32* xferBytes, CARDCallback callback)
{
    GcCardControl* card;
    u8* wa;
    u16* dir[2];
    u16* fat[2];
    u16* map;
    s32 result;
    s32 errors;
    s32 currentFat = 0;
    s32 currentDir = 0;
    s32 fileNo;
    u16 iBlock;
    u16 cBlock;
    u16 cFree;
    int updateFat = 0;
    int updateDir = 0;
    int updateOrphan = 0;
    int valid;

    if (xferBytes) {
        *xferBytes = 0;
    }

    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    result = VerifyID(card);
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }

    errors = VerifyDir(card, &currentDir);
    errors += VerifyFAT(card, &currentFat);
    if (errors > 1) {
        return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
    }

    wa = (u8*)(uintptr_t)card->work_area;
    dir[0] = (u16*)(wa + (u32)(1u + 0u) * CARD_SYSTEM_BLOCK_SIZE);
    dir[1] = (u16*)(wa + (u32)(1u + 1u) * CARD_SYSTEM_BLOCK_SIZE);
    fat[0] = (u16*)(wa + (u32)(3u + 0u) * CARD_SYSTEM_BLOCK_SIZE);
    fat[1] = (u16*)(wa + (u32)(3u + 1u) * CARD_SYSTEM_BLOCK_SIZE);

    if (errors == 1) {
        if (!card->current_dir_ptr) {
            card->current_dir_ptr = (uintptr_t)dir[currentDir];
            memcpy((void*)dir[currentDir], (void*)dir[currentDir ^ 1u], CARD_SYSTEM_BLOCK_SIZE);
            updateDir = 1;
        }
        else {
            card->current_fat_ptr = (uintptr_t)fat[currentFat];
            memcpy((void*)fat[currentFat], (void*)fat[currentFat ^ 1u], CARD_SYSTEM_BLOCK_SIZE);
            updateFat = 1;
        }
    }

    map = fat[currentFat ^ 1u];
    memset(map, 0, CARD_SYSTEM_BLOCK_SIZE);

    valid = card->current_dir_ptr ? 1 : 0;
    for (fileNo = 0; fileNo < (s32)PORT_CARD_MAX_FILE; fileNo++) {
        u8* ent = (u8*)((uintptr_t)dir[0] + (uintptr_t)fileNo * PORT_CARD_DIR_SIZE);
        if (!valid) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
        if (ent[PORT_CARD_DIR_OFF_GAMENAME] == 0xFFu) {
            continue;
        }

        u16 entStartBlock = rd16be((u8*)(uintptr_t)(ent + PORT_CARD_DIR_OFF_STARTBLOCK));
        u16 entLength = rd16be((u8*)(uintptr_t)(ent + PORT_CARD_DIR_OFF_LENGTH));
        for (iBlock = entStartBlock, cBlock = 0u;
             iBlock != 0xFFFFu && cBlock < entLength;
             iBlock = ((u16*)fat[currentFat])[iBlock], ++cBlock) {
            if (!card_is_valid_block_no(card, iBlock) || ((u32)++map[iBlock] > 1u)) {
                return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
            }
        }
        if (cBlock != entLength || iBlock != 0xFFFFu) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
    }

    cFree = 0u;
    for (iBlock = CARD_NUM_SYSTEM_BLOCK; iBlock < (u16)card->cblock; iBlock++) {
        u16 nextBlock = ((u16*)fat[currentFat])[iBlock];
        if (map[iBlock] == 0u) {
            if (nextBlock != PORT_CARD_FAT_AVAIL) {
                ((u16*)fat[currentFat])[iBlock] = PORT_CARD_FAT_AVAIL;
                updateOrphan = 1;
            }
            cFree++;
        }
        else if (!card_is_valid_block_no(card, nextBlock) && nextBlock != 0xFFFFu) {
            return __CARDPutControlBlock(card, CARD_RESULT_BROKEN);
        }
    }
    if (cFree != ((u16*)fat[currentFat])[PORT_CARD_FAT_FREEBLOCKS]) {
        ((u16*)fat[currentFat])[PORT_CARD_FAT_FREEBLOCKS] = cFree;
        updateOrphan = 1;
    }
    if (updateOrphan) {
        __CARDCheckSum(
            &((u8*)fat[currentFat])[PORT_CARD_FAT_CHECKCODE * 2u],
            (int)(CARD_SYSTEM_BLOCK_SIZE - 4u),
            &((u16*)fat[currentFat])[PORT_CARD_FAT_CHECKSUM],
            &((u16*)fat[currentFat])[PORT_CARD_FAT_CHECKSUMINV]
        );
    }
    memcpy(fat[currentFat ^ 1u], fat[currentFat], CARD_SYSTEM_BLOCK_SIZE);

    if (updateDir) {
        if (xferBytes) {
            *xferBytes = CARD_SYSTEM_BLOCK_SIZE;
        }
        return __CARDUpdateDir(chan, callback);
    }
    if (updateFat || updateOrphan) {
        if (xferBytes) {
            *xferBytes = CARD_SYSTEM_BLOCK_SIZE;
        }
        return __CARDUpdateFatBlock(chan, fat[currentFat], callback);
    }

    __CARDPutControlBlock(card, CARD_RESULT_READY);
    if (callback) {
        callback(chan, CARD_RESULT_READY);
    }
    return CARD_RESULT_READY;
}

s32 CARDCheckAsync(s32 chan, CARDCallback callback)
{
    s32 xferBytes;
    (void)xferBytes;
    return CARDCheckExAsync(chan, &xferBytes, callback);
}

void __CARDSyncCallback(s32 chan, s32 result)
{
    (void)chan;
    (void)result;
}

s32 __CARDSync(s32 chan)
{
    if (chan < 0 || chan >= 2) {
        return CARD_RESULT_FATAL_ERROR;
    }
    return __CARDBlock[chan].result;
}

s32 CARDCheck(s32 chan)
{
    s32 xferBytes;
    s32 result = CARDCheckExAsync(chan, &xferBytes, __CARDSyncCallback);
    if (result >= 0) {
        return __CARDSync(chan);
    }
    return result;
}

static void cb_record(s32 chan, s32 result)
{
    (void)chan;
    g_oracle_cb_calls++;
    g_oracle_cb_last = result;
}

static void make_dir_block(u8* base, s16 checkCode, u8 seed)
{
    for (u32 i = 0u; i < CARD_SYSTEM_BLOCK_SIZE; i++) {
        base[i] = (u8)(seed ^ (u8)i);
    }
    for (u32 i = 0u; i < (u32)PORT_CARD_MAX_FILE; i++) {
        base[i * (u32)PORT_CARD_DIR_SIZE] = 0xFFu;
    }
    u8* chk = base + (u32)PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE;
    wr16be(chk + (PORT_CARD_DIR_SIZE - 6u), (u16)checkCode);
    u16 cs = 0;
    u16 csi = 0;
    card_checksum_u16be(base, CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 4u), cs);
    wr16be(chk + (PORT_CARD_DIR_SIZE - 2u), csi);
}

static void make_fat_block(u8* base, u16 checkCode, u16 cBlock)
{
    memset(base, 0x00u, CARD_SYSTEM_BLOCK_SIZE);
    for (u16 block = (u16)CARD_NUM_SYSTEM_BLOCK; block < cBlock; block++) {
        wr16be(base + (u32)block * 2u, PORT_CARD_FAT_AVAIL);
    }
    wr16be(base + (u32)PORT_CARD_FAT_CHECKCODE * 2u, checkCode);
    wr16be(base + (u32)PORT_CARD_FAT_FREEBLOCKS * 2u, (u16)(cBlock - (u16)CARD_NUM_SYSTEM_BLOCK));
    u16 cs = 0;
    u16 csi = 0;
    card_checksum_u16be(base + (u32)PORT_CARD_FAT_CHECKCODE * 2u, CARD_SYSTEM_BLOCK_SIZE - 4u, &cs, &csi);
    wr16be(base + (u32)PORT_CARD_FAT_CHECKSUM * 2u, cs);
    wr16be(base + (u32)PORT_CARD_FAT_CHECKSUMINV * 2u, csi);
}

static void make_id_block(u8* base, u16 size_u16, u16 encode, const u8* flash_id12)
{
    memset(base, 0, 512u);
    for (u32 i = 0u; i < 12u; i++) {
        base[i] = flash_id12[i];
    }
    wr16be(base + 32u, 0);
    wr16be(base + 34u, size_u16);
    wr16be(base + 36u, encode);
    u16 cs = 0;
    u16 csi = 0;
    card_checksum_u16be(base, 512u - 4u, &cs, &csi);
    wr16be(base + 508u, cs);
    wr16be(base + 510u, csi);
}

static void seed_oracle_state(const u8 flash_id12[12])
{
    s_oracle_font_encode = 0u;
    memset(&s_sram_ex, 0, sizeof(s_sram_ex));
    memcpy(s_sram_ex.flashID[0], flash_id12, 12u);
    s_sram_ex.flashIDCheckSum[0] = 0u;
    s_sram_ex.flashIDCheckSum[1] = 0u;
}

static void reset_card_state(u8* work, u16 size_u16, s32 cblock)
{
    memset(__CARDBlock, 0, sizeof(__CARDBlock));
    __CARDBlock[0].disk_id = 0x80000000u;
    __CARDBlock[0].attached = 1;
    __CARDBlock[0].result = CARD_RESULT_READY;
    __CARDBlock[0].size_u16 = size_u16;
    __CARDBlock[0].cblock = (u32)cblock;
    __CARDBlock[0].work_area = (uintptr_t)work;
    __CARDBlock[0].current_dir_ptr = 0u;
    __CARDBlock[0].current_fat_ptr = 0u;
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
    const GcCardControl* card = &__CARDBlock[0];
    const u8* work = (const u8*)(uintptr_t)card->work_area;

    wr32be(out + *w * 4u, tag); (*w)++;
    wr32be(out + *w * 4u, (u32)rc); (*w)++;
    wr32be(out + *w * 4u, xferBytes); (*w)++;
    wr32be(out + *w * 4u, (u32)card->result); (*w)++;
    wr32be(out + *w * 4u, (u32)card->current_dir_ptr); (*w)++;
    wr32be(out + *w * 4u, (u32)card->current_fat_ptr); (*w)++;
    wr32be(out + *w * 4u, (u32)cbCalls); (*w)++;
    wr32be(out + *w * 4u, (u32)cbLast); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x0000u, 512u)); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x2000u, CARD_SYSTEM_BLOCK_SIZE)); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x4000u, CARD_SYSTEM_BLOCK_SIZE)); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x6000u, CARD_SYSTEM_BLOCK_SIZE)); (*w)++;
    wr32be(out + *w * 4u, fnv1a32(work + 0x8000u, CARD_SYSTEM_BLOCK_SIZE)); (*w)++;
}

void oracle_card_check_pbt_001_suite(u8* out)
{
    static const u8 flash0[12] = {
        0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u,
        0x16u, 0x17u, 0x18u, 0x19u, 0x1Au, 0x1Bu
    };
    const u16 cBlock = 64u;
    u8* work = (u8*)0x81000000u;
    u32 w = 0u;

    memset(out, 0, 0x200u);
    wr32be(out + (w * 4u), 0x43434B30u); w++;
    wr32be(out + (w * 4u), 0x00000000u); w++;

    seed_oracle_state(flash0);

    // Case 0x1: CARDCheck() success path.
    memset(work, 0, 5u * CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, (s32)cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, cBlock);
    make_fat_block(work + 0x8000u, 2u, cBlock);

    g_oracle_cb_calls = 0;
    g_oracle_cb_last = 0;
    s32 rc = CARDCheck(0);
    dump_case(out, &w, 0x43434B31u, rc, 0u, g_oracle_cb_calls, g_oracle_cb_last);

    // Case 0x2: CARDCheckExAsync(one bad dir) -> updates DIR and callback.
    memset(work, 0, 5u * CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, (s32)cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, cBlock);
    make_fat_block(work + 0x8000u, 2u, cBlock);
    wr16be(work + 0x2000u + (u32)PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE + (PORT_CARD_DIR_SIZE - 4u), 0x1234u);

    s32 xfer = 0u;
    g_oracle_cb_calls = 0;
    g_oracle_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &w, 0x43434B32u, rc, (u32)xfer, g_oracle_cb_calls, g_oracle_cb_last);

    // Case 0x3: CARDCheckExAsync(one bad FAT) -> updates FAT and callback.
    memset(work, 0, 5u * CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, (s32)cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, cBlock);
    make_fat_block(work + 0x8000u, 2u, cBlock);
    wr16be(work + 0x6000u + (u32)PORT_CARD_FAT_CHECKSUM * 2u, 0x1234u);

    xfer = 0u;
    g_oracle_cb_calls = 0;
    g_oracle_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &w, 0x43434B33u, rc, (u32)xfer, g_oracle_cb_calls, g_oracle_cb_last);

    // Case 0x4: CARDCheckExAsync(two errors) -> BROKEN.
    memset(work, 0, 5u * CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, (s32)cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, cBlock);
    make_fat_block(work + 0x8000u, 2u, cBlock);
    wr16be(work + 0x2000u + (u32)PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE + (PORT_CARD_DIR_SIZE - 2u), 0x1234u);
    wr16be(work + 0x6000u + (u32)PORT_CARD_FAT_CHECKSUMINV * 2u, 0x1234u);

    xfer = 0u;
    g_oracle_cb_calls = 0;
    g_oracle_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &w, 0x43434B34u, rc, (u32)xfer, g_oracle_cb_calls, g_oracle_cb_last);

    // Case 0x5: CARDCheckExAsync(encoding mismatch) -> ENCODING.
    memset(work, 0, 5u * CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, (s32)cBlock);
    make_id_block(work + 0x0000u, 0x1234u, 1u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, cBlock);
    make_fat_block(work + 0x8000u, 2u, cBlock);
    xfer = 0u;
    g_oracle_cb_calls = 0;
    g_oracle_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &w, 0x43434B35u, rc, (u32)xfer, g_oracle_cb_calls, g_oracle_cb_last);

    // Case 0x6: CARDCheckExAsync(attach missing) -> NOCARD.
    memset(work, 0, 5u * CARD_SYSTEM_BLOCK_SIZE);
    reset_card_state(work, 0x1234u, (s32)cBlock);
    __CARDBlock[0].attached = 0;
    make_id_block(work + 0x0000u, 0x1234u, 0u, flash0);
    make_dir_block(work + 0x2000u, 1u, 0xA0u);
    make_dir_block(work + 0x4000u, 2u, 0xB0u);
    make_fat_block(work + 0x6000u, 1u, cBlock);
    make_fat_block(work + 0x8000u, 2u, cBlock);

    xfer = 0u;
    g_oracle_cb_calls = 0;
    g_oracle_cb_last = 0;
    rc = CARDCheckExAsync(0, &xfer, cb_record);
    dump_case(out, &w, 0x43434B36u, rc, (u32)xfer, g_oracle_cb_calls, g_oracle_cb_last);
}
