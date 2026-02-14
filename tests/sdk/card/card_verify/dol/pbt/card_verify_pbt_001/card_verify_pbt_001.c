#include <stdint.h>
#include <string.h>
#include "src/sdk_port/gc_mem.c"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

// Oracle symbols.
extern u32 oracle_font_encode;
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
extern OSSramEx oracle_sramex;

enum { CARD_FILENAME_MAX = 32 };
typedef struct CARDDir {
  u8 gameName[4];
  u8 company[2];
  u8 _padding0;
  u8 bannerFormat;
  u8 fileName[CARD_FILENAME_MAX];
  u32 time;
  u32 iconAddr;
  u16 iconFormat;
  u16 iconSpeed;
  u8 permission;
  u8 copyTimes;
  u16 startBlock;
  u16 length;
  u8 _padding1[2];
  u32 commentAddr;
} CARDDir;

typedef struct CARDDirCheck {
  u8 padding0[64 - 2 * 4];
  u16 padding1;
  int16_t checkCode;
  u16 checkSum;
  u16 checkSumInv;
} CARDDirCheck;

typedef struct CARDID {
  u8 serial[32];
  u16 deviceID;
  u16 size;
  u16 encode;
  u8 padding[512 - 32 - 5 * 2];
  u16 checkSum;
  u16 checkSumInv;
} CARDID;

typedef struct CARDControl {
  int attached;
  s32 result;
  u16 size;
  u16 pageSize;
  s32 sectorSize;
  u16 cBlock;
  u16 vendorID;
  s32 latency;
  u8 id[12];
  void* workArea;
  CARDDir* currentDir;
  u16* currentFat;
} CARDControl;
extern CARDControl __CARDBlock[2];

void oracle___CARDCheckSum(void* ptr, int length, u16* checksum, u16* checksumInv);
s32 oracle___CARDVerify(CARDControl* card);

enum { CARD_SYSTEM_BLOCK_SIZE = 8 * 1024, CARD_MAX_FILE = 127 };
enum { CARD_RESULT_READY = 0, CARD_RESULT_BROKEN = -6, CARD_RESULT_ENCODING = -13 };
enum { CARD_NUM_SYSTEM_BLOCK = 5 };
enum { CARD_FAT_AVAIL = 0x0000u, CARD_FAT_CHECKSUM = 0, CARD_FAT_CHECKSUMINV = 1, CARD_FAT_CHECKCODE = 2, CARD_FAT_FREEBLOCKS = 3 };

static inline void wr32be(volatile u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}

static u32 fnv1a32(const u8* p, u32 n) {
  u32 h = 2166136261u;
  for (u32 i = 0; i < n; i++) {
    h ^= (u32)p[i];
    h *= 16777619u;
  }
  return h;
}

static CARDDirCheck* dir_check(CARDDir* dir0) {
  return (CARDDirCheck*)&dir0[CARD_MAX_FILE];
}

static void make_dir_block(u8* base, int checkcode, u8 seed) {
  // Fill the whole block deterministically, then set checksum fields.
  for (u32 i = 0; i < CARD_SYSTEM_BLOCK_SIZE; i++) base[i] = (u8)(seed ^ (u8)i);
  CARDDir* dir = (CARDDir*)base;
  CARDDirCheck* chk = dir_check(dir);
  chk->checkCode = (int16_t)checkcode;
  u16 cs = 0, csi = 0;
  oracle___CARDCheckSum(dir, CARD_SYSTEM_BLOCK_SIZE - 4, &cs, &csi);
  chk->checkSum = cs;
  chk->checkSumInv = csi;
}

static void make_fat_block(u8* base, int checkcode, u16 cblock) {
  u16* fat = (u16*)base;
  // Clear, then mark all user blocks as available.
  for (u32 i = 0; i < (CARD_SYSTEM_BLOCK_SIZE / 2u); i++) fat[i] = 0;
  for (u16 b = CARD_NUM_SYSTEM_BLOCK; b < cblock; b++) fat[b] = CARD_FAT_AVAIL;
  fat[CARD_FAT_CHECKCODE] = (u16)checkcode;
  fat[CARD_FAT_FREEBLOCKS] = (u16)(cblock - CARD_NUM_SYSTEM_BLOCK);

  u16 cs = 0, csi = 0;
  oracle___CARDCheckSum(&fat[CARD_FAT_CHECKCODE], CARD_SYSTEM_BLOCK_SIZE - 4, &cs, &csi);
  fat[CARD_FAT_CHECKSUM] = cs;
  fat[CARD_FAT_CHECKSUMINV] = csi;
}

static void make_id(CARDID* id, u16 size, u16 encode, const u8* flash_id12) {
  memset(id, 0, 512);
  id->deviceID = 0;
  id->size = size;
  id->encode = encode;
  // Seed rand=0 by leaving serial[12..19]==0.
  for (u32 i = 0; i < 12u; i++) id->serial[i] = flash_id12[i];
  u16 cs = 0, csi = 0;
  oracle___CARDCheckSum(id, (int)sizeof(*id) - 4, &cs, &csi);
  id->checkSum = cs;
  id->checkSumInv = csi;
}

int main(void) {
  gc_mem_set(0x80000000u, 0x01800000u, (u8*)0x80000000u);

  volatile u8* out = (volatile u8*)0x80300000u;
  for (u32 i = 0; i < 0x80u; i++) out[i] = 0;
  u32 off = 0;

  static u8 work[5 * 8 * 1024];
  memset(work, 0, sizeof(work));

  // Init oracle SRAM + font encode.
  oracle_font_encode = 0;
  for (u32 i = 0; i < 12u; i++) {
    oracle_sramex.flashID[0][i] = (u8)(0x10u + (u8)i);
    oracle_sramex.flashID[1][i] = (u8)(0x80u + (u8)i);
  }

  memset(__CARDBlock, 0, sizeof(__CARDBlock));
  CARDControl* card = &__CARDBlock[0];
  card->attached = 1;
  card->size = 0x1234;
  card->cBlock = 0x20;
  card->workArea = work;
  card->currentDir = 0;
  card->currentFat = 0;

  // Pointers into workArea.
  CARDID* id = (CARDID*)(work + 0x0000);
  u8* dir0 = work + 0x2000;
  u8* dir1 = work + 0x4000;
  u8* fat0 = work + 0x6000;
  u8* fat1 = work + 0x8000;

  // Case A: pass + select/copy.
  make_id(id, card->size, (u16)oracle_font_encode, oracle_sramex.flashID[0]);
  make_dir_block(dir0, 1, 0xA0);
  make_dir_block(dir1, 2, 0xB0);
  make_fat_block(fat0, 1, card->cBlock);
  make_fat_block(fat1, 2, card->cBlock);
  s32 rc_a = oracle___CARDVerify(card);
  u32 dir0_h = fnv1a32(dir0, CARD_SYSTEM_BLOCK_SIZE);
  u32 fat0_h = fnv1a32(fat0, CARD_SYSTEM_BLOCK_SIZE);
  wr32be(out + off, 0x43564641u); off += 4; // "CVFA"
  wr32be(out + off, (u32)rc_a); off += 4;
  wr32be(out + off, (u32)(card->currentDir == (CARDDir*)dir0)); off += 4;
  wr32be(out + off, (u32)(card->currentFat == (u16*)fat0)); off += 4;
  wr32be(out + off, dir0_h); off += 4;
  wr32be(out + off, fat0_h); off += 4;

  // Case B: ID checksum fail -> BROKEN.
  card->currentDir = 0;
  card->currentFat = 0;
  make_id(id, card->size, (u16)oracle_font_encode, oracle_sramex.flashID[0]);
  id->checkSum ^= 0x1234;
  s32 rc_b = oracle___CARDVerify(card);
  wr32be(out + off, 0x43564642u); off += 4; // "CVFB"
  wr32be(out + off, (u32)rc_b); off += 4;

  // Case C: encoding fail.
  card->currentDir = 0;
  card->currentFat = 0;
  make_id(id, card->size, (u16)(oracle_font_encode ^ 1u), oracle_sramex.flashID[0]);
  s32 rc_c = oracle___CARDVerify(card);
  wr32be(out + off, 0x43564643u); off += 4; // "CVFC"
  wr32be(out + off, (u32)rc_c); off += 4;

  wr32be(out + 0x00, 0x43564630u); // "CVF0"

  while (1) { __asm__ volatile("nop"); }
  return 0;
}
