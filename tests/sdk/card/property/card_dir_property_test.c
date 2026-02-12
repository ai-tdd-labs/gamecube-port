/*
 * card_dir_property_test.c --- Property-based parity test for CARD directory ops.
 *
 * Oracle:  exact copy of decomp (native structs) in card_dir_oracle.h
 * Port:    sdk_port/card/card_dir.c (CARDDir in gc_mem, big-endian)
 *
 * Test levels:
 *   L0 — __CARDCompareFileName: random filenames, parity on return value
 *   L1 — __CARDAccess + __CARDIsPublic: random dir entries, parity on result
 *   L2 — __CARDGetFileNo: random directory, search for files, parity on fileNo
 *   L3 — __CARDSeek: random FAT chains, seek to offsets, parity on fileInfo
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Oracle (native structs) */
#include "card_dir_oracle.h"

/* Port (gc_mem big-endian) */
#include "card_dir.h"
#include "gc_mem.h"

/* ── xorshift32 PRNG ── */
static uint32_t g_rng;
static uint32_t xorshift32(void) {
    uint32_t x = g_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g_rng = x;
    return x;
}

/* ── gc_mem layout (single contiguous region) ── */
#define GC_MEM_BASE   0x80200000u
#define GC_DIR_OFFSET 0u
#define GC_FAT_OFFSET 0x4000u  /* dir is 127*64=8128 bytes, pad to 0x4000=16384 */
#define GC_DIR_BASE   (GC_MEM_BASE + GC_DIR_OFFSET)
#define GC_DIR_SIZE   (PORT_CARD_MAX_FILE * PORT_CARD_DIR_SIZE)
#define GC_FAT_BASE   (GC_MEM_BASE + GC_FAT_OFFSET)
#define GC_FAT_SIZE   (4096 * 2)
#define GC_MEM_SIZE   (GC_FAT_OFFSET + GC_FAT_SIZE)

static uint8_t gc_backing[GC_MEM_SIZE];

/* ── Test infrastructure ── */
static int g_verbose;
static int g_pass, g_fail;

#define CHECK(cond, ...) do { \
    if (!(cond)) { \
        printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); \
        g_fail++; return; \
    } else { g_pass++; } \
} while (0)

/* ── gc_mem helpers ── */

static void store_u8_gc(uint32_t addr, uint8_t val)
{
    uint8_t *p = gc_mem_ptr(addr, 1);
    *p = val;
}

static void store_u16be_gc(uint32_t addr, uint16_t val)
{
    uint8_t *p = gc_mem_ptr(addr, 2);
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)(val);
}

static void store_u32be_gc(uint32_t addr, uint32_t val)
{
    uint8_t *p = gc_mem_ptr(addr, 4);
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8);
    p[3] = (uint8_t)(val);
}

/* Write an oracle_CARDDir entry to gc_mem at the given dir index */
static void write_dir_entry_to_gcmem(const oracle_CARDDir *ent, int index)
{
    uint32_t base = GC_DIR_BASE + (uint32_t)index * PORT_CARD_DIR_SIZE;
    uint8_t *p;
    int i;

    /* Byte fields — write directly */
    p = gc_mem_ptr(base + PORT_CARD_DIR_OFF_GAMENAME, 4);
    memcpy(p, ent->gameName, 4);

    p = gc_mem_ptr(base + PORT_CARD_DIR_OFF_COMPANY, 2);
    memcpy(p, ent->company, 2);

    /* padding0 + bannerFormat */
    store_u8_gc(base + 6, ent->_padding0);
    store_u8_gc(base + 7, ent->bannerFormat);

    /* fileName[32] */
    p = gc_mem_ptr(base + PORT_CARD_DIR_OFF_FILENAME, 32);
    memcpy(p, ent->fileName, 32);

    /* Multi-byte fields — big-endian */
    store_u32be_gc(base + 40, ent->time);
    store_u32be_gc(base + 44, ent->iconAddr);
    store_u16be_gc(base + 48, ent->iconFormat);
    store_u16be_gc(base + 50, ent->iconSpeed);

    store_u8_gc(base + PORT_CARD_DIR_OFF_PERMISSION, ent->permission);
    store_u8_gc(base + 53, ent->copyTimes);
    store_u16be_gc(base + PORT_CARD_DIR_OFF_STARTBLOCK, ent->startBlock);
    store_u16be_gc(base + PORT_CARD_DIR_OFF_LENGTH, ent->length);

    for (i = 0; i < 2; i++)
        store_u8_gc(base + 58 + (uint32_t)i, ent->_padding1[i]);
    store_u32be_gc(base + 60, ent->commentAddr);
}

/* Write FAT entry to gc_mem */
static void write_fat_entry_gc(uint16_t index, uint16_t val)
{
    store_u16be_gc(GC_FAT_BASE + (uint32_t)index * 2, val);
}

static void init_gc_mem_regions(void)
{
    memset(gc_backing, 0, sizeof(gc_backing));
    gc_mem_set(GC_MEM_BASE, GC_MEM_SIZE, gc_backing);
}

/* Generate a random filename (1..31 chars + NUL) */
static void gen_random_filename(char *buf)
{
    int len = (int)(xorshift32() % 31) + 1;
    int i;
    for (i = 0; i < len; i++) {
        buf[i] = 'A' + (char)(xorshift32() % 26);
    }
    buf[len] = '\0';
    /* Fill rest with 0 */
    for (i = len + 1; i < 32; i++) buf[i] = '\0';
}

/* Generate a random CARDDir entry */
static void gen_random_dir_entry(oracle_CARDDir *ent, int empty)
{
    int i;
    memset(ent, 0, sizeof(*ent));

    if (empty) {
        /* Mark as empty: gameName[0] = 0xFF */
        memset(ent->gameName, 0xFF, 4);
        return;
    }

    /* gameName — 4 bytes, not starting with 0xFF */
    for (i = 0; i < 4; i++) {
        ent->gameName[i] = (uint8_t)(xorshift32() % 0xFE + 1);
    }
    /* company — 2 bytes */
    ent->company[0] = (uint8_t)(xorshift32() % 256);
    ent->company[1] = (uint8_t)(xorshift32() % 256);

    /* fileName */
    gen_random_filename((char *)ent->fileName);

    /* permission */
    ent->permission = (uint8_t)(xorshift32() & 0xFF);

    /* startBlock, length — set by caller for seek tests */
    ent->startBlock = 0xFFFF;
    ent->length = 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * L0: __CARDCompareFileName parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L0_compare_filename(void)
{
    oracle_CARDDir ent;
    char search[33];
    int o_result, p_result;

    memset(&ent, 0, sizeof(ent));

    /* Test case 1: exact match */
    gen_random_filename((char *)ent.fileName);
    memcpy(search, ent.fileName, 32);
    search[32] = '\0';

    init_gc_mem_regions();
    write_dir_entry_to_gcmem(&ent, 0);

    o_result = oracle_CARDCompareFileName(&ent, search);
    p_result = port_CARDCompareFileName(GC_DIR_BASE, search);
    CHECK(o_result == p_result,
          "L0 exact match: oracle=%d port=%d", o_result, p_result);

    /* Test case 2: different name */
    gen_random_filename(search);
    /* Make sure it's different */
    if (memcmp(search, ent.fileName, 32) == 0) {
        search[0] = (search[0] == 'A') ? 'B' : 'A';
    }

    o_result = oracle_CARDCompareFileName(&ent, search);
    p_result = port_CARDCompareFileName(GC_DIR_BASE, search);
    CHECK(o_result == p_result,
          "L0 different name: oracle=%d port=%d", o_result, p_result);

    /* Test case 3: prefix match (search is shorter) */
    memcpy(search, ent.fileName, 5);
    search[5] = '\0';
    /* Only matches if ent.fileName is exactly 5 chars */

    o_result = oracle_CARDCompareFileName(&ent, search);
    p_result = port_CARDCompareFileName(GC_DIR_BASE, search);
    CHECK(o_result == p_result,
          "L0 prefix: oracle=%d port=%d", o_result, p_result);

    /* Test case 4: empty string */
    search[0] = '\0';
    o_result = oracle_CARDCompareFileName(&ent, search);
    p_result = port_CARDCompareFileName(GC_DIR_BASE, search);
    CHECK(o_result == p_result,
          "L0 empty search: oracle=%d port=%d", o_result, p_result);

    /* Test case 5: max-length name (32 chars, no NUL in fileName) */
    {
        int i;
        for (i = 0; i < 32; i++) {
            ent.fileName[i] = 'A' + (uint8_t)(xorshift32() % 26);
        }
        memcpy(search, ent.fileName, 32);
        search[32] = '\0';

        init_gc_mem_regions();
        write_dir_entry_to_gcmem(&ent, 0);

        o_result = oracle_CARDCompareFileName(&ent, search);
        p_result = port_CARDCompareFileName(GC_DIR_BASE, search);
        CHECK(o_result == p_result,
              "L0 maxlen: oracle=%d port=%d", o_result, p_result);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L1: __CARDAccess + __CARDIsPublic parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L1_access_public(void)
{
    oracle_CARDDir ent;
    oracle_CARDDirCtl ocard;
    oracle_DVDDiskID diskID;
    port_CARDDirControl pcard;
    int32_t o_access, p_access, o_public, p_public;

    init_gc_mem_regions();

    /* Generate a random entry */
    gen_random_dir_entry(&ent, 0);
    write_dir_entry_to_gcmem(&ent, 0);

    /* Set up oracle card with matching diskID */
    memcpy(diskID.gameName, ent.gameName, 4);
    memcpy(diskID.company, ent.company, 2);
    ocard.attached = 1;
    ocard.diskID = &diskID;
    ocard.currentDir = &ent;

    /* Port card with matching diskID */
    pcard.attached = 1;
    pcard.diskID_is_none = 0;
    memcpy(pcard.gameName, ent.gameName, 4);
    memcpy(pcard.company, ent.company, 2);
    pcard.dir_addr = GC_DIR_BASE;

    /* Test Access with matching game */
    o_access = oracle_CARDAccess(&ocard, &ent);
    p_access = port_CARDAccess(&pcard, GC_DIR_BASE);
    CHECK(o_access == p_access,
          "L1 access-match: oracle=%d port=%d", o_access, p_access);

    /* Test Access with different game */
    diskID.gameName[0] ^= 0x42;
    pcard.gameName[0] ^= 0x42;
    o_access = oracle_CARDAccess(&ocard, &ent);
    p_access = port_CARDAccess(&pcard, GC_DIR_BASE);
    CHECK(o_access == p_access,
          "L1 access-mismatch: oracle=%d port=%d", o_access, p_access);

    /* Test Access with DiskNone (any file accessible) */
    ocard.diskID = &oracle_CARDDiskNone;
    pcard.diskID_is_none = 1;
    o_access = oracle_CARDAccess(&ocard, &ent);
    p_access = port_CARDAccess(&pcard, GC_DIR_BASE);
    CHECK(o_access == p_access,
          "L1 access-disknone: oracle=%d port=%d", o_access, p_access);

    /* Test Access with empty entry (gameName[0] = 0xFF) */
    {
        oracle_CARDDir empty_ent;
        gen_random_dir_entry(&empty_ent, 1);
        write_dir_entry_to_gcmem(&empty_ent, 1);

        o_access = oracle_CARDAccess(&ocard, &empty_ent);
        p_access = port_CARDAccess(&pcard, GC_DIR_BASE + PORT_CARD_DIR_SIZE);
        CHECK(o_access == p_access,
              "L1 access-empty: oracle=%d port=%d", o_access, p_access);
    }

    /* Test IsPublic */
    o_public = oracle_CARDIsPublic(&ent);
    p_public = port_CARDIsPublic(GC_DIR_BASE);
    CHECK(o_public == p_public,
          "L1 public: oracle=%d port=%d perm=0x%02x",
          o_public, p_public, ent.permission);

    /* Test IsPublic with public flag set */
    ent.permission |= ORACLE_CARD_ATTR_PUBLIC;
    write_dir_entry_to_gcmem(&ent, 0);
    o_public = oracle_CARDIsPublic(&ent);
    p_public = port_CARDIsPublic(GC_DIR_BASE);
    CHECK(o_public == p_public,
          "L1 public-set: oracle=%d port=%d", o_public, p_public);
}

/* ═══════════════════════════════════════════════════════════════════
 * L2: __CARDGetFileNo parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L2_getfileno(void)
{
    oracle_CARDDir dir[ORACLE_CARD_MAX_FILE];
    oracle_CARDDirCtl ocard;
    oracle_DVDDiskID diskID;
    port_CARDDirControl pcard;
    int32_t o_fileNo, p_fileNo;
    int32_t o_result, p_result;
    char search[33];
    int n_files, target_idx, i;

    init_gc_mem_regions();
    memset(dir, 0xFF, sizeof(dir)); /* All empty initially */

    /* Populate some random files (5..20) */
    n_files = (int)(xorshift32() % 16 + 5);

    /* Choose game identity */
    for (i = 0; i < 4; i++) diskID.gameName[i] = (uint8_t)(xorshift32() % 0xFE + 1);
    for (i = 0; i < 2; i++) diskID.company[i] = (uint8_t)(xorshift32() % 256);

    for (i = 0; i < n_files; i++) {
        int slot = (int)(xorshift32() % ORACLE_CARD_MAX_FILE);
        /* Avoid overwriting a used slot */
        while (dir[slot].gameName[0] != 0xFF) {
            slot = (slot + 1) % ORACLE_CARD_MAX_FILE;
        }
        gen_random_dir_entry(&dir[slot], 0);
        memcpy(dir[slot].gameName, diskID.gameName, 4);
        memcpy(dir[slot].company, diskID.company, 2);
    }

    /* Write all entries to gc_mem */
    for (i = 0; i < ORACLE_CARD_MAX_FILE; i++) {
        write_dir_entry_to_gcmem(&dir[i], i);
    }

    /* Set up oracle */
    ocard.attached = 1;
    ocard.diskID = &diskID;
    ocard.currentDir = dir;
    ocard.cBlock = 256;
    ocard.sectorSize = 8192;

    /* Set up port */
    pcard.attached = 1;
    pcard.diskID_is_none = 0;
    memcpy(pcard.gameName, diskID.gameName, 4);
    memcpy(pcard.company, diskID.company, 2);
    pcard.dir_addr = GC_DIR_BASE;
    pcard.fat_addr = GC_FAT_BASE;
    pcard.cBlock = 256;
    pcard.sectorSize = 8192;

    /* Search for a file that exists */
    target_idx = -1;
    for (i = 0; i < ORACLE_CARD_MAX_FILE; i++) {
        if (dir[i].gameName[0] != 0xFF) {
            target_idx = i;
            break;
        }
    }
    if (target_idx >= 0) {
        memcpy(search, dir[target_idx].fileName, 32);
        search[32] = '\0';
        /* Find the NUL if present */
        for (i = 0; i < 32; i++) {
            if (search[i] == '\0') break;
        }

        o_fileNo = -1; p_fileNo = -1;
        o_result = oracle_CARDGetFileNo(&ocard, search, &o_fileNo);
        p_result = port_CARDGetFileNo(&pcard, search, &p_fileNo);
        CHECK(o_result == p_result,
              "L2 found: oracle_rc=%d port_rc=%d search=%s",
              o_result, p_result, search);
        CHECK(o_fileNo == p_fileNo,
              "L2 fileNo: oracle=%d port=%d", o_fileNo, p_fileNo);
    }

    /* Search for a file that doesn't exist */
    {
        char fake[33] = "ZZZZ_NONEXISTENT_FILE_12345";
        o_fileNo = -1; p_fileNo = -1;
        o_result = oracle_CARDGetFileNo(&ocard, fake, &o_fileNo);
        p_result = port_CARDGetFileNo(&pcard, fake, &p_fileNo);
        CHECK(o_result == p_result,
              "L2 notfound: oracle_rc=%d port_rc=%d",
              o_result, p_result);
    }

    /* Search with card not attached */
    ocard.attached = 0;
    pcard.attached = 0;
    o_result = oracle_CARDGetFileNo(&ocard, "test", &o_fileNo);
    p_result = port_CARDGetFileNo(&pcard, "test", &p_fileNo);
    CHECK(o_result == p_result,
          "L2 nocard: oracle_rc=%d port_rc=%d", o_result, p_result);
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: __CARDSeek parity
 * ═══════════════════════════════════════════════════════════════════ */
static void test_L3_seek(void)
{
    oracle_CARDDir dir[ORACLE_CARD_MAX_FILE];
    uint16_t fat[4096];
    oracle_CARDDirCtl ocard;
    oracle_DVDDiskID diskID;
    port_CARDDirControl pcard;
    oracle_CARDFileInfo o_fi;
    port_CARDFileInfo p_fi;
    int32_t o_result, p_result;
    uint16_t cBlock;
    int32_t sectorSize;
    int file_slot, chain_len, i;
    uint16_t cur_block;

    init_gc_mem_regions();
    memset(dir, 0xFF, sizeof(dir));
    memset(fat, 0, sizeof(fat));

    cBlock = 128;
    sectorSize = 8192;

    /* Set up identity */
    for (i = 0; i < 4; i++) diskID.gameName[i] = (uint8_t)(xorshift32() % 0xFE + 1);
    for (i = 0; i < 2; i++) diskID.company[i] = (uint8_t)(xorshift32() % 256);

    /* Create a file with a FAT chain */
    file_slot = 0;
    gen_random_dir_entry(&dir[file_slot], 0);
    memcpy(dir[file_slot].gameName, diskID.gameName, 4);
    memcpy(dir[file_slot].company, diskID.company, 2);

    /* Chain length: 3..20 blocks */
    chain_len = (int)(xorshift32() % 18 + 3);
    if (chain_len + 5 > cBlock) chain_len = cBlock - 5;

    dir[file_slot].length = (uint16_t)chain_len;
    dir[file_slot].startBlock = 5; /* first data block */

    /* Build a linear FAT chain: 5 -> 6 -> 7 -> ... -> (5+chain_len-1) -> 0xFFFF */
    for (i = 0; i < chain_len - 1; i++) {
        fat[5 + i] = (uint16_t)(5 + i + 1);
    }
    fat[5 + chain_len - 1] = 0xFFFF;

    /* Write dir and FAT to gc_mem */
    for (i = 0; i < ORACLE_CARD_MAX_FILE; i++) {
        write_dir_entry_to_gcmem(&dir[i], i);
    }
    for (i = 0; i < 4096; i++) {
        write_fat_entry_gc((uint16_t)i, fat[i]);
    }

    /* Set up oracle card */
    ocard.attached = 1;
    ocard.diskID = &diskID;
    ocard.currentDir = dir;
    ocard.currentFat = fat;
    ocard.cBlock = cBlock;
    ocard.sectorSize = sectorSize;

    /* Set up port card */
    pcard.attached = 1;
    pcard.diskID_is_none = 0;
    memcpy(pcard.gameName, diskID.gameName, 4);
    memcpy(pcard.company, diskID.company, 2);
    pcard.dir_addr = GC_DIR_BASE;
    pcard.fat_addr = GC_FAT_BASE;
    pcard.cBlock = cBlock;
    pcard.sectorSize = sectorSize;

    /* Test seek from start to various offsets */
    {
        int32_t offset, length;
        int32_t file_size = chain_len * sectorSize;
        int test_count;

        for (test_count = 0; test_count < 5; test_count++) {
            /* Random valid offset (sector-aligned for simplicity) */
            offset = (int32_t)((xorshift32() % (uint32_t)(chain_len)) * (uint32_t)sectorSize);
            length = sectorSize; /* Read one sector */
            if (offset + length > file_size) {
                length = file_size - offset;
            }
            if (length <= 0) continue;

            /* Initialize fileInfo: at start of file */
            o_fi.chan = 0;
            o_fi.fileNo = file_slot;
            o_fi.offset = 0;
            o_fi.length = 0;
            o_fi.iBlock = dir[file_slot].startBlock;
            o_fi.__padding = 0;

            p_fi.chan = 0;
            p_fi.fileNo = file_slot;
            p_fi.offset = 0;
            p_fi.length = 0;
            p_fi.iBlock = dir[file_slot].startBlock;

            o_result = oracle_CARDSeek(&o_fi, length, offset, &ocard);
            p_result = port_CARDSeek(&p_fi, length, offset, &pcard);

            CHECK(o_result == p_result,
                  "L3 seek rc: offset=%d len=%d oracle=%d port=%d",
                  offset, length, o_result, p_result);

            if (o_result == 0) {
                CHECK(o_fi.offset == p_fi.offset,
                      "L3 seek offset: oracle=%d port=%d",
                      o_fi.offset, p_fi.offset);
                CHECK(o_fi.iBlock == p_fi.iBlock,
                      "L3 seek iBlock: oracle=%u port=%u",
                      (unsigned)o_fi.iBlock, (unsigned)p_fi.iBlock);
                CHECK(o_fi.length == p_fi.length,
                      "L3 seek length: oracle=%d port=%d",
                      o_fi.length, p_fi.length);
            }
        }
    }

    /* Test seek beyond file bounds (should return LIMIT) */
    {
        int32_t file_size = chain_len * sectorSize;
        o_fi.chan = 0;
        o_fi.fileNo = file_slot;
        o_fi.offset = 0;
        o_fi.length = 0;
        o_fi.iBlock = dir[file_slot].startBlock;
        o_fi.__padding = 0;

        p_fi.chan = 0;
        p_fi.fileNo = file_slot;
        p_fi.offset = 0;
        p_fi.length = 0;
        p_fi.iBlock = dir[file_slot].startBlock;

        o_result = oracle_CARDSeek(&o_fi, sectorSize, file_size, &ocard);
        p_result = port_CARDSeek(&p_fi, sectorSize, file_size, &pcard);
        CHECK(o_result == p_result,
              "L3 seek-limit: oracle=%d port=%d", o_result, p_result);
    }

    /* Test seek with non-linear FAT chain */
    {
        uint16_t blocks[32];
        int n_blocks = (int)(xorshift32() % 10 + 3);
        if (n_blocks + 5 > cBlock) n_blocks = cBlock - 5;

        /* Clear old chain */
        for (i = 5; i < cBlock; i++) fat[i] = 0;

        /* Build a shuffled chain */
        for (i = 0; i < n_blocks; i++) {
            blocks[i] = (uint16_t)(5 + i);
        }
        /* Simple shuffle */
        for (i = n_blocks - 1; i > 0; i--) {
            int j = (int)(xorshift32() % (uint32_t)(i + 1));
            uint16_t tmp = blocks[i];
            blocks[i] = blocks[j];
            blocks[j] = tmp;
        }

        /* Link the chain */
        for (i = 0; i < n_blocks - 1; i++) {
            fat[blocks[i]] = blocks[i + 1];
        }
        fat[blocks[n_blocks - 1]] = 0xFFFF;

        dir[file_slot].startBlock = blocks[0];
        dir[file_slot].length = (uint16_t)n_blocks;

        /* Rewrite to gc_mem */
        write_dir_entry_to_gcmem(&dir[file_slot], file_slot);
        for (i = 0; i < 4096; i++) {
            write_fat_entry_gc((uint16_t)i, fat[i]);
        }

        /* Seek to middle of file */
        {
            int32_t offset = (int32_t)((xorshift32() % (uint32_t)n_blocks) * (uint32_t)sectorSize);
            int32_t length = sectorSize;
            int32_t file_size = n_blocks * sectorSize;
            if (offset + length > file_size) length = file_size - offset;
            if (length <= 0) length = sectorSize, offset = 0;

            o_fi.chan = 0;
            o_fi.fileNo = file_slot;
            o_fi.offset = 0;
            o_fi.length = 0;
            o_fi.iBlock = dir[file_slot].startBlock;
            o_fi.__padding = 0;

            p_fi.chan = 0;
            p_fi.fileNo = file_slot;
            p_fi.offset = 0;
            p_fi.length = 0;
            p_fi.iBlock = dir[file_slot].startBlock;

            o_result = oracle_CARDSeek(&o_fi, length, offset, &ocard);
            p_result = port_CARDSeek(&p_fi, length, offset, &pcard);

            CHECK(o_result == p_result,
                  "L3 shuffle-seek rc: offset=%d oracle=%d port=%d",
                  offset, o_result, p_result);
            if (o_result == 0) {
                CHECK(o_fi.offset == p_fi.offset,
                      "L3 shuffle-seek offset: oracle=%d port=%d",
                      o_fi.offset, p_fi.offset);
                CHECK(o_fi.iBlock == p_fi.iBlock,
                      "L3 shuffle-seek iBlock: oracle=%u port=%u",
                      (unsigned)o_fi.iBlock, (unsigned)p_fi.iBlock);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════════ */
int main(int argc, char **argv)
{
    int num_runs = 2000;
    uint32_t seed = 12345;
    int i;
    g_verbose = 0;

    for (i = 1; i < argc; i++) {
        if (sscanf(argv[i], "--seed=%u", &seed) == 1) continue;
        if (sscanf(argv[i], "--num-runs=%d", &num_runs) == 1) continue;
        if (strcmp(argv[i], "-v") == 0) { g_verbose = 1; continue; }
    }

    printf("card_dir_property_test: seed=%u num_runs=%d\n", seed, num_runs);

    for (i = 0; i < num_runs; i++) {
        g_rng = seed + (uint32_t)i;

        test_L0_compare_filename();
        test_L1_access_public();
        test_L2_getfileno();
        test_L3_seek();
    }

    printf("\ncard_dir_property_test: %d/%d PASS", g_pass, g_pass + g_fail);
    if (g_fail) printf(", %d FAIL", g_fail);
    printf("\n");

    return g_fail ? 1 : 0;
}
