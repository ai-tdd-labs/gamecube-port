#include <stdint.h>
#include <string.h>

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

enum { MAX_CHAN = 3 };

// Knobs/state.
u32 oracle_exi_select_ok[MAX_CHAN] = {1, 1, 1};
u32 oracle_exi_locked[MAX_CHAN];
u32 oracle_exi_selected[MAX_CHAN];

u32 oracle_exi_card_status[MAX_CHAN];
u32 oracle_exi_card_status_reads[MAX_CHAN];
u32 oracle_exi_card_status_clears[MAX_CHAN];

u32 oracle_exi_last_imm_len[MAX_CHAN];
u32 oracle_exi_last_imm_type[MAX_CHAN];
u32 oracle_exi_last_imm_data[MAX_CHAN]; // MSB-first packed.

static u32 pending_status_read[MAX_CHAN];

static inline u32 pack_msb_first(const u8* b, int len) {
  u32 d = 0;
  for (int i = 0; i < len; i++) d |= (u32)b[i] << ((3 - (u32)i) * 8);
  return d;
}

void oracle_EXIInit(void) {
  memset(oracle_exi_locked, 0, sizeof(oracle_exi_locked));
  memset(oracle_exi_selected, 0, sizeof(oracle_exi_selected));
  memset(oracle_exi_card_status, 0, sizeof(oracle_exi_card_status));
  memset(oracle_exi_card_status_reads, 0, sizeof(oracle_exi_card_status_reads));
  memset(oracle_exi_card_status_clears, 0, sizeof(oracle_exi_card_status_clears));
  memset(oracle_exi_last_imm_len, 0, sizeof(oracle_exi_last_imm_len));
  memset(oracle_exi_last_imm_type, 0, sizeof(oracle_exi_last_imm_type));
  memset(oracle_exi_last_imm_data, 0, sizeof(oracle_exi_last_imm_data));
  memset(pending_status_read, 0, sizeof(pending_status_read));
}

BOOL EXILock(s32 channel, u32 device, void* cb) {
  (void)device;
  (void)cb;
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  if (oracle_exi_locked[channel]) return FALSE;
  oracle_exi_locked[channel] = 1;
  return TRUE;
}

BOOL EXIUnlock(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  oracle_exi_locked[channel] = 0;
  return TRUE;
}

BOOL EXISelect(s32 channel, u32 device, u32 freq) {
  (void)device;
  (void)freq;
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  if (!oracle_exi_locked[channel]) return FALSE;
  if (!oracle_exi_select_ok[channel]) return FALSE;
  if (oracle_exi_selected[channel]) return FALSE;
  oracle_exi_selected[channel] = 1;
  return TRUE;
}

BOOL EXIDeselect(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  if (!oracle_exi_selected[channel]) return FALSE;
  oracle_exi_selected[channel] = 0;
  return TRUE;
}

BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, void* cb) {
  (void)cb;
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  if (!oracle_exi_selected[channel]) return FALSE;
  if (!buffer || length <= 0 || length > 4) return FALSE;

  oracle_exi_last_imm_len[channel] = (u32)length;
  oracle_exi_last_imm_type[channel] = type;
  oracle_exi_last_imm_data[channel] = 0;

  if (type == EXI_WRITE) {
    const u8* b = (const u8*)buffer;
    u32 data = pack_msb_first(b, (int)length);
    oracle_exi_last_imm_data[channel] = data;
    if (b[0] == 0x83u) {
      pending_status_read[channel] = 1;
    } else if (b[0] == 0x89u) {
      pending_status_read[channel] = 0;
      // Preserve non-error bits (inferred from MP4 SDK usage).
      oracle_exi_card_status[channel] &= ~0x18u;
      oracle_exi_card_status_clears[channel]++;
    }
    return TRUE;
  }

  // EXI_READ
  if (type == EXI_READ) {
    u8* out = (u8*)buffer;
    if (length == 1 && pending_status_read[channel]) {
      pending_status_read[channel] = 0;
      oracle_exi_card_status_reads[channel]++;
      out[0] = (u8)(oracle_exi_card_status[channel] & 0xFFu);
      oracle_exi_last_imm_data[channel] = ((u32)out[0]) << 24;
      return TRUE;
    }
    out[0] = 0;
    oracle_exi_last_imm_data[channel] = 0;
    return TRUE;
  }

  return FALSE;
}

BOOL EXISync(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  return TRUE;
}
