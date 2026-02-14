#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define RTC_SRAM_SIZE 64u

typedef struct OSSram {
  u16 checkSum;
  u16 checkSumInv;
  u32 ead0;
  u32 ead1;
  u32 counterBias;
  int8_t displayOffsetH;
  u8 ntd;
  u8 language;
  u8 flags;
} OSSram;

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

typedef struct {
  u8 sram[RTC_SRAM_SIZE];
  u32 offset;
  BOOL enabled;
  BOOL locked;
  BOOL sync;
} ScbT;

static ScbT Scb;

// Deterministic knobs (test-only): allow modeling ReadSram/WriteSram failure.
u32 oracle_os_sram_read_ok = 1;
u32 oracle_os_sram_write_ok = 1;

// Test-only instrumentation: export modeled internal state.
u32 oracle_os_scb_locked;
u32 oracle_os_scb_enabled;
u32 oracle_os_scb_sync;
u32 oracle_os_scb_offset;
u32 oracle_os_sram_read_calls;
u32 oracle_os_sram_write_calls;

static inline void update_mirrors(void) {
  oracle_os_scb_locked = (u32)(Scb.locked != FALSE);
  oracle_os_scb_enabled = (u32)(Scb.enabled != FALSE);
  oracle_os_scb_sync = (u32)(Scb.sync != FALSE);
  oracle_os_scb_offset = Scb.offset;
}

static int OSDisableInterrupts(void) {
  // For deterministic unit tests, treat "enabled" as always 1.
  return 1;
}

static int OSRestoreInterrupts(int level) {
  // Return prior state (unused by oracle).
  return level;
}

static BOOL ReadSram(u8* buffer) {
  oracle_os_sram_read_calls++;
  if (!oracle_os_sram_read_ok) {
    return FALSE;
  }
  memset(buffer, 0, RTC_SRAM_SIZE);
  return TRUE;
}

static BOOL WriteSram(const u8* buffer, u32 offset, u32 size) {
  (void)buffer;
  (void)offset;
  (void)size;
  oracle_os_sram_write_calls++;
  return oracle_os_sram_write_ok ? TRUE : FALSE;
}

void oracle___OSInitSram(void) {
  Scb.locked = Scb.enabled = FALSE;
  Scb.sync = ReadSram(Scb.sram);
  Scb.offset = RTC_SRAM_SIZE;
  update_mirrors();
}

static void* LockSram(u32 offset) {
  BOOL enabled = OSDisableInterrupts();

  if (Scb.locked != FALSE) {
    OSRestoreInterrupts(enabled);
    update_mirrors();
    return NULL;
  }

  Scb.enabled = enabled;
  Scb.locked = TRUE;
  update_mirrors();
  return Scb.sram + offset;
}

OSSram* oracle___OSLockSram(void) { return (OSSram*)LockSram(0); }
OSSramEx* oracle___OSLockSramEx(void) { return (OSSramEx*)LockSram((u32)sizeof(OSSram)); }

static BOOL UnlockSram(BOOL commit, u32 offset) {
  u16* p;

  if (commit) {
    if (offset == 0) {
      OSSram* sram = (OSSram*)Scb.sram;
      sram->checkSum = 0;
      sram->checkSumInv = 0;
      for (p = (u16*)&sram->counterBias; p < (u16*)(Scb.sram + sizeof(OSSram)); p++) {
        sram->checkSum += *p;
        sram->checkSumInv += (u16)~*p;
      }
    }

    if (offset < Scb.offset) {
      Scb.offset = offset;
    }

    Scb.sync = WriteSram(Scb.sram + Scb.offset, Scb.offset, RTC_SRAM_SIZE - Scb.offset);
    if (Scb.sync) {
      Scb.offset = RTC_SRAM_SIZE;
    }
  }

  Scb.locked = FALSE;
  OSRestoreInterrupts(Scb.enabled);
  update_mirrors();
  return Scb.sync;
}

BOOL oracle___OSUnlockSram(BOOL commit) { return UnlockSram(commit, 0); }
BOOL oracle___OSUnlockSramEx(BOOL commit) { return UnlockSram(commit, (u32)sizeof(OSSram)); }

BOOL oracle___OSSyncSram(void) {
  update_mirrors();
  return Scb.sync;
}

