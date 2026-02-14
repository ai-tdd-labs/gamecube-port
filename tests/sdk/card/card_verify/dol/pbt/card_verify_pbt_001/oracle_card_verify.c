#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;
typedef uint64_t OSTime;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BROKEN = -6,
  CARD_RESULT_ENCODING = -13,
};

enum {
  CARD_SYSTEM_BLOCK_SIZE = 8 * 1024,
  CARD_NUM_SYSTEM_BLOCK = 5,
  CARD_MAX_FILE = 127,
  CARD_FILENAME_MAX = 32,
};

enum {
  CARD_FAT_AVAIL = 0x0000u,
  CARD_FAT_CHECKSUM = 0x0000u,
  CARD_FAT_CHECKSUMINV = 0x0001u,
  CARD_FAT_CHECKCODE = 0x0002u,
  CARD_FAT_FREEBLOCKS = 0x0003u,
};

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

// Deterministic knobs.
u32 oracle_font_encode;
OSSramEx oracle_sramex;

static OSSramEx* __OSLockSramEx(void) { return &oracle_sramex; }
static void __OSUnlockSramEx(BOOL commit) { (void)commit; }
static u16 OSGetFontEncode(void) { return (u16)(oracle_font_encode & 0xFFFFu); }

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
  BOOL attached;
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

CARDControl __CARDBlock[2];

static inline CARDDirCheck* __CARDGetDirCheck(CARDDir* dir) {
  return (CARDDirCheck*)&dir[CARD_MAX_FILE];
}

void oracle___CARDCheckSum(void* ptr, int length, u16* checksum, u16* checksumInv) {
  u16* p;
  int i;

  length /= (int)sizeof(u16);
  *checksum = *checksumInv = 0;
  for (i = 0, p = (u16*)ptr; i < length; i++, p++) {
    *checksum += *p;
    *checksumInv += (u16)~*p;
  }
  if (*checksum == 0xFFFF) *checksum = 0;
  if (*checksumInv == 0xFFFF) *checksumInv = 0;
}

static s32 VerifyID(CARDControl* card) {
  CARDID* id;
  u16 checksum;
  u16 checksumInv;
  OSSramEx* sramEx;
  OSTime rand;
  int i;

  id = (CARDID*)card->workArea;

  if (id->deviceID != 0 || id->size != card->size) return CARD_RESULT_BROKEN;

  oracle___CARDCheckSum(id, (int)sizeof(CARDID) - (int)sizeof(u32), &checksum, &checksumInv);
  if (id->checkSum != checksum || id->checkSumInv != checksumInv) return CARD_RESULT_BROKEN;

  if (id->encode != OSGetFontEncode()) return CARD_RESULT_ENCODING;

  rand = *(OSTime*)&id->serial[12];
  sramEx = __OSLockSramEx();
  for (i = 0; i < 12; i++) {
    rand = (rand * 1103515245ull + 12345ull) >> 16;
    if (id->serial[i] != (u8)(sramEx->flashID[card - __CARDBlock][i] + (u8)rand)) {
      __OSUnlockSramEx(FALSE);
      return CARD_RESULT_BROKEN;
    }
    rand = ((rand * 1103515245ull + 12345ull) >> 16) & 0x7FFFull;
  }
  __OSUnlockSramEx(FALSE);

  return CARD_RESULT_READY;
}

static s32 VerifyDir(CARDControl* card) {
  CARDDir* dir[2];
  CARDDirCheck* check[2];
  u16 checkSum;
  u16 checkSumInv;
  int i;
  int errors;
  int current;

  current = errors = 0;
  for (i = 0; i < 2; i++) {
    dir[i] = (CARDDir*)((u8*)card->workArea + (1 + i) * CARD_SYSTEM_BLOCK_SIZE);
    check[i] = __CARDGetDirCheck(dir[i]);
    oracle___CARDCheckSum(dir[i], CARD_SYSTEM_BLOCK_SIZE - (int)sizeof(u32), &checkSum, &checkSumInv);
    if (check[i]->checkSum != checkSum || check[i]->checkSumInv != checkSumInv) {
      ++errors;
      current = i;
      card->currentDir = 0;
    }
  }

  if (errors == 0) {
    if (card->currentDir == 0) {
      current = ((check[0]->checkCode - check[1]->checkCode) < 0) ? 0 : 1;
      card->currentDir = dir[current];
      memcpy(dir[current], dir[current ^ 1], CARD_SYSTEM_BLOCK_SIZE);
    } else {
      current = (card->currentDir == dir[0]) ? 0 : 1;
    }
  }
  return (s32)errors;
}

static s32 VerifyFAT(CARDControl* card) {
  u16* fat[2];
  u16* fatp;
  u16 nBlock;
  u16 cFree;
  int i;
  u16 checkSum;
  u16 checkSumInv;
  int errors;
  int current;

  current = errors = 0;
  for (i = 0; i < 2; i++) {
    fatp = fat[i] = (u16*)((u8*)card->workArea + (3 + i) * CARD_SYSTEM_BLOCK_SIZE);
    oracle___CARDCheckSum(&fatp[CARD_FAT_CHECKCODE], CARD_SYSTEM_BLOCK_SIZE - (int)sizeof(u32), &checkSum, &checkSumInv);
    if (fatp[CARD_FAT_CHECKSUM] != checkSum || fatp[CARD_FAT_CHECKSUMINV] != checkSumInv) {
      ++errors;
      current = i;
      card->currentFat = 0;
      continue;
    }

    cFree = 0;
    for (nBlock = CARD_NUM_SYSTEM_BLOCK; nBlock < card->cBlock; nBlock++) {
      if (fatp[nBlock] == CARD_FAT_AVAIL) cFree++;
    }
    if (cFree != fatp[CARD_FAT_FREEBLOCKS]) {
      ++errors;
      current = i;
      card->currentFat = 0;
      continue;
    }
  }

  if (errors == 0) {
    if (card->currentFat == 0) {
      current = (((int16_t)fat[0][CARD_FAT_CHECKCODE] - (int16_t)fat[1][CARD_FAT_CHECKCODE]) < 0) ? 0 : 1;
      card->currentFat = fat[current];
      memcpy(fat[current], fat[current ^ 1], CARD_SYSTEM_BLOCK_SIZE);
    } else {
      current = (card->currentFat == fat[0]) ? 0 : 1;
    }
  }
  return (s32)errors;
}

s32 oracle___CARDVerify(CARDControl* card) {
  s32 result;
  int errors;

  result = VerifyID(card);
  if (result < 0) return result;

  errors = (int)VerifyDir(card);
  errors += (int)VerifyFAT(card);

  switch (errors) {
    case 0: return CARD_RESULT_READY;
    case 1: return CARD_RESULT_BROKEN;
    default: return CARD_RESULT_BROKEN;
  }
}

