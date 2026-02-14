#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define EXI_READ 0
#define EXI_WRITE 1

enum { MAX_CHAN = 3 };

enum { EXI_STATE_LOCKED = 0x10 };

u32 oracle_exi_select_ok[MAX_CHAN] = {1, 1, 1};
u32 oracle_exi_state[MAX_CHAN];

u32 oracle_exi_last_imm_len[MAX_CHAN];
u32 oracle_exi_last_imm_type[MAX_CHAN];
u32 oracle_exi_last_imm_data[MAX_CHAN];

static inline u32 pack_msb_first(const u8* b, int len) {
  u32 d = 0;
  for (int i = 0; i < len; i++) d |= (u32)b[i] << ((3 - (u32)i) * 8);
  return d;
}

void oracle_EXIInit(void) {
  memset(oracle_exi_state, 0, sizeof(oracle_exi_state));
  memset(oracle_exi_last_imm_len, 0, sizeof(oracle_exi_last_imm_len));
  memset(oracle_exi_last_imm_type, 0, sizeof(oracle_exi_last_imm_type));
  memset(oracle_exi_last_imm_data, 0, sizeof(oracle_exi_last_imm_data));
}

BOOL EXILock(s32 channel, u32 device, void* cb) {
  (void)device; (void)cb;
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  if (oracle_exi_state[channel] & EXI_STATE_LOCKED) return FALSE;
  oracle_exi_state[channel] |= EXI_STATE_LOCKED;
  return TRUE;
}

BOOL EXIUnlock(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  oracle_exi_state[channel] &= ~EXI_STATE_LOCKED;
  return TRUE;
}

BOOL EXISelect(s32 channel, u32 device, u32 frequency) {
  (void)device; (void)frequency;
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  if ((oracle_exi_state[channel] & EXI_STATE_LOCKED) == 0) return FALSE;
  return oracle_exi_select_ok[channel] ? TRUE : FALSE;
}

BOOL EXIDeselect(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  return TRUE;
}

BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, void* cb) {
  (void)cb;
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  if (!buffer || length <= 0 || length > 4) return FALSE;
  oracle_exi_last_imm_len[channel] = (u32)length;
  oracle_exi_last_imm_type[channel] = type;
  oracle_exi_last_imm_data[channel] = (type == EXI_WRITE) ? pack_msb_first((const u8*)buffer, length) : 0;
  return TRUE;
}

BOOL EXISync(s32 channel) { (void)channel; return TRUE; }
