#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int BOOL;
typedef int32_t s32;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define EXI_READ 0
#define EXI_WRITE 1

BOOL EXISelect(s32 channel, u32 device, u32 frequency);
BOOL EXIDeselect(s32 channel);
BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, void* cb);
BOOL EXISync(s32 channel);

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_NOCARD = -3,
};

s32 oracle___CARDReadStatus(s32 chan, u8* status) {
  BOOL err;
  u8 cmd[2];

  if (!EXISelect(chan, 0, 4)) {
    return CARD_RESULT_NOCARD;
  }

  cmd[0] = 0x83;
  cmd[1] = 0x00;

  err = FALSE;
  err |= !EXIImm(chan, cmd, 2, EXI_WRITE, 0);
  err |= !EXISync(chan);
  err |= !EXIImm(chan, status, 1, EXI_READ, 0);
  err |= !EXISync(chan);
  err |= !EXIDeselect(chan);
  return err ? CARD_RESULT_NOCARD : CARD_RESULT_READY;
}

s32 oracle___CARDClearStatus(s32 chan) {
  BOOL err;
  u8 cmd;

  if (!EXISelect(chan, 0, 4)) {
    return CARD_RESULT_NOCARD;
  }

  cmd = 0x89;
  err = FALSE;
  err |= !EXIImm(chan, &cmd, 1, EXI_WRITE, 0);
  err |= !EXISync(chan);
  err |= !EXIDeselect(chan);
  return err ? CARD_RESULT_NOCARD : CARD_RESULT_READY;
}

