#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define EXI_WRITE 1

enum { CARD_RESULT_READY = 0, CARD_RESULT_NOCARD = -3 };

BOOL EXISelect(s32 channel, u32 device, u32 frequency);
BOOL EXIDeselect(s32 channel);
BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, void* cb);
BOOL EXISync(s32 channel);

s32 oracle___CARDEnableInterrupt(s32 chan, BOOL enable) {
  BOOL err;
  u8 cmd[2];

  if (!EXISelect(chan, 0, 4)) {
    return CARD_RESULT_NOCARD;
  }

  cmd[0] = 0x81;
  cmd[1] = enable ? 0x01 : 0x00;
  err = FALSE;
  err |= !EXIImm(chan, cmd, 2, EXI_WRITE, 0);
  err |= !EXISync(chan);
  err |= !EXIDeselect(chan);

  return err ? CARD_RESULT_NOCARD : CARD_RESULT_READY;
}
