#include <stdint.h>

typedef uint64_t u64;
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint16_t u16;
typedef int16_t s16;
typedef int BOOL;

#define TRUE 1
#define FALSE 0

#include "src/sdk_port/card/card_bios.h"

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_BROKEN = -6,
  CARD_RESULT_FATAL_ERROR = -128,
  CARD_RESULT_BUSY = -1,
  CARD_RESULT_NOCARD = -3
};

typedef struct __attribute__((packed)) CARDID {
  u8 serial[32];
  u16 deviceID;
  u16 size;
  u16 encode;
  u8 padding[470];
  u16 checkSum;
  u16 checkSumInv;
} CARDID;

GcCardControl gc_card_block[GC_CARD_CHANS];

s32 __CARDPutControlBlock(GcCardControl *card, s32 result)
{
  if (card->attached || card->result == CARD_RESULT_BUSY) {
    card->result = result;
  }
  return result;
}

s32 __CARDGetControlBlock(s32 chan, GcCardControl **pcard)
{
  GcCardControl *card;

  if (chan < 0 || chan >= 2 || gc_card_block[chan].disk_id == 0) {
    return CARD_RESULT_FATAL_ERROR;
  }

  card = &gc_card_block[chan];
  if (!card->attached) {
    return CARD_RESULT_NOCARD;
  }
  if (card->result == CARD_RESULT_BUSY) {
    return CARD_RESULT_BUSY;
  }

  card->result = CARD_RESULT_BUSY;
  card->api_callback = 0;
  if (pcard) {
    *pcard = card;
  }
  return CARD_RESULT_READY;
}

s32 oracle_CARDGetSerialNo(s32 chan, u64* serialNo)
{
  GcCardControl* card;
  s32 result = __CARDGetControlBlock(chan, &card);
  if (result < 0) {
    return result;
  }

  CARDID* id = (CARDID*)(uintptr_t)card->work_area;
  u64 code = 0;
  for (int i = 0; i < 4; i++) {
    u64 piece = 0;
    for (int b = 0; b < 8; b++) {
      piece = (piece << 8) | (u64)id->serial[i * 8u + b];
    }
    code ^= piece;
  }

  if (serialNo) {
    *serialNo = code;
  }
  return __CARDPutControlBlock(card, CARD_RESULT_READY);
}
