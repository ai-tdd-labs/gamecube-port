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

#define RESULT_BASE ((volatile u8*)0x80300000u)

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

void oracle___OSInitSram(void);
OSSram* oracle___OSLockSram(void);
OSSramEx* oracle___OSLockSramEx(void);
BOOL oracle___OSUnlockSram(BOOL commit);
BOOL oracle___OSUnlockSramEx(BOOL commit);
BOOL oracle___OSSyncSram(void);

extern u32 oracle_os_sram_read_ok;
extern u32 oracle_os_sram_write_ok;
extern u32 oracle_os_scb_locked;
extern u32 oracle_os_scb_enabled;
extern u32 oracle_os_scb_sync;
extern u32 oracle_os_scb_offset;
extern u32 oracle_os_sram_read_calls;
extern u32 oracle_os_sram_write_calls;

static inline void wr32be(volatile u8* p, u32 v) {
  p[0] = (u8)(v >> 24);
  p[1] = (u8)(v >> 16);
  p[2] = (u8)(v >> 8);
  p[3] = (u8)v;
}

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }

static u32 hash_u32(u32 h, u32 v) { return rotl1(h) ^ v; }

static u32 hash_bytes(u32 h, const void* data, u32 n) {
  const u8* p = (const u8*)data;
  for (u32 i = 0; i < n; i++) {
    h = rotl1(h) ^ p[i];
  }
  return h;
}

static u32 snap_ex(u32 h) {
  OSSramEx* ex = oracle___OSLockSramEx();
  h = hash_u32(h, (u32)(ex != NULL));
  if (ex) {
    h = hash_bytes(h, ex->flashID, sizeof(ex->flashID));
    h = hash_bytes(h, ex->flashIDCheckSum, sizeof(ex->flashIDCheckSum));
  }
  (void)oracle___OSUnlockSramEx(FALSE);
  return h;
}

int main(void) {
  volatile u8* out = RESULT_BASE;
  u32 off = 0;
  for (u32 i = 0; i < 0x60u; i++) out[i] = 0;

  // L0: basic lock semantics + persistence of writes.
  oracle_os_sram_read_ok = 1;
  oracle_os_sram_write_ok = 1;
  oracle___OSInitSram();

  u32 l0 = 0;
  OSSramEx* ex1 = oracle___OSLockSramEx();
  l0 = hash_u32(l0, (u32)(ex1 != NULL));
  OSSramEx* ex2 = oracle___OSLockSramEx();
  l0 = hash_u32(l0, (u32)(ex2 == NULL));
  if (ex1) {
    for (u32 i = 0; i < 12; i++) ex1->flashID[0][i] = (u8)(0xA0u + i);
    ex1->flashIDCheckSum[0] = 0x5Au;
  }
  l0 = hash_u32(l0, (u32)oracle___OSUnlockSramEx(FALSE));
  l0 = snap_ex(l0);
  l0 = hash_u32(l0, (u32)oracle___OSUnlockSramEx(TRUE)); // should unlock even if already unlocked? no: lock state differs; keep deterministic

  // Re-lock and ensure data persists.
  OSSramEx* ex3 = oracle___OSLockSramEx();
  l0 = hash_u32(l0, (u32)(ex3 != NULL));
  if (ex3) {
    l0 = hash_u32(l0, (u32)ex3->flashID[0][0]);
    l0 = hash_u32(l0, (u32)ex3->flashIDCheckSum[0]);
  }
  l0 = hash_u32(l0, (u32)oracle___OSUnlockSramEx(TRUE));

  // L1: write failure keeps offset sticky until a successful commit.
  oracle___OSInitSram();
  u32 l1 = 0;
  oracle_os_sram_write_ok = 0;
  OSSramEx* exf = oracle___OSLockSramEx();
  if (exf) exf->flashIDCheckSum[1] = 0x99u;
  l1 = hash_u32(l1, (u32)oracle___OSUnlockSramEx(TRUE));
  l1 = hash_u32(l1, oracle_os_scb_sync);
  l1 = hash_u32(l1, oracle_os_scb_offset);
  oracle_os_sram_write_ok = 1;
  OSSramEx* exf2 = oracle___OSLockSramEx();
  if (exf2) exf2->flashIDCheckSum[1] ^= 0x01u;
  l1 = hash_u32(l1, (u32)oracle___OSUnlockSramEx(TRUE));
  l1 = hash_u32(l1, oracle_os_scb_sync);
  l1 = hash_u32(l1, oracle_os_scb_offset);

  // L2: cross-locking Sram vs SramEx shares the same lock.
  oracle___OSInitSram();
  u32 l2 = 0;
  OSSram* s0 = oracle___OSLockSram();
  l2 = hash_u32(l2, (u32)(s0 != NULL));
  OSSramEx* ex_blocked = oracle___OSLockSramEx();
  l2 = hash_u32(l2, (u32)(ex_blocked == NULL));
  if (s0) {
    s0->flags = 0x80u;
  }
  l2 = hash_u32(l2, (u32)oracle___OSUnlockSram(TRUE));
  OSSram* s1 = oracle___OSLockSram();
  if (s1) {
    l2 = hash_u32(l2, (u32)s1->flags);
  }
  (void)oracle___OSUnlockSram(FALSE);

  // L5: boundary invariant: second unlock without lock should not re-lock, but returns last sync.
  u32 l5 = 0;
  oracle___OSInitSram();
  l5 = hash_u32(l5, (u32)oracle___OSUnlockSramEx(FALSE));
  l5 = hash_u32(l5, (u32)oracle___OSUnlockSramEx(FALSE));
  l5 = hash_u32(l5, oracle_os_scb_locked);

  u32 master = 0;
  master = hash_u32(master, l0);
  master = hash_u32(master, l1);
  master = hash_u32(master, l2);
  master = hash_u32(master, l5);

  wr32be(out + off, 0x4F534558u); off += 4; // "OSEX"
  wr32be(out + off, master); off += 4;
  wr32be(out + off, l0); off += 4;
  wr32be(out + off, l1); off += 4;
  wr32be(out + off, l2); off += 4;
  wr32be(out + off, l5); off += 4;

  // Dump a small deterministic state sample for debugging diffs.
  wr32be(out + off, oracle_os_sram_read_calls); off += 4;
  wr32be(out + off, oracle_os_sram_write_calls); off += 4;
  wr32be(out + off, oracle_os_scb_sync); off += 4;
  wr32be(out + off, oracle_os_scb_offset); off += 4;
  wr32be(out + off, oracle_os_scb_locked); off += 4;
  wr32be(out + off, oracle_os_scb_enabled); off += 4;

  // Persisted flashID sample.
  OSSramEx* ex_final = oracle___OSLockSramEx();
  if (ex_final) {
    for (u32 i = 0; i < 12; i++) out[off++] = ex_final->flashID[0][i];
    for (u32 i = 0; i < 2; i++) out[off++] = ex_final->flashIDCheckSum[i];
  }
  (void)oracle___OSUnlockSramEx(FALSE);

  while (1) {
  }
  return 0;
}
