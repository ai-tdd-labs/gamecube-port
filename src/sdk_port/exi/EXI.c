#include "dolphin/exi.h"

#include <stdint.h>
#include <string.h>

// Minimal, deterministic EXI model for host testing.
//
// Scope: enough state/behavior for CARD stack unit tests and replay harnesses.
// This is not a hardware emulator. It models:
// - lock/selected/busy flags
// - immediate transfers by packing/unpacking the IMM register
// - a small set of observable EXI regs in a flat array matching hw_regs.h layout
//
// Ground truth should be established via Dolphin-oracle suites.

typedef uint32_t u32;
typedef int32_t s32;

enum {
  MAX_CHAN = 3,
  REG_MAX = 5,
};

// Flattened EXI MMIO registers (mirrors dolphin/hw_regs.h: __EXIRegs[16]).
// Channel layout: STAT, DMA_ADDR, LEN, CONTROL, IMM.
u32 gc_exi_regs[16];

typedef struct GcExiControl {
  EXICallback exi_cb;
  EXICallback tc_cb;
  EXICallback ext_cb;
  u32 state;
  int imm_len;
  uint8_t* imm_buf;
  u32 dev;
} GcExiControl;

static GcExiControl s_ecb[MAX_CHAN];

static inline volatile u32* regp(s32 chan, int idx) {
  return &gc_exi_regs[(u32)chan * REG_MAX + (u32)idx];
}

static inline u32 exi_0cr(u32 tstart, u32 dma, u32 rw, u32 tlen) {
  return ((tstart & 1u) << 0) | ((dma & 1u) << 1) | ((rw & 1u) << 2) | ((tlen & 0xFu) << 4);
}

static void complete_transfer(s32 chan) {
  GcExiControl* exi = &s_ecb[chan];
  if ((exi->state & EXI_STATE_BUSY) == 0) {
    return;
  }

  if ((exi->state & EXI_STATE_IMM) && exi->imm_len > 0 && exi->imm_buf) {
    // When the transfer completes, the IMM register contains up to 4 bytes.
    u32 data = *regp(chan, 4);
    for (int i = 0; i < exi->imm_len; i++) {
      exi->imm_buf[i] = (uint8_t)((data >> ((3 - i) * 8)) & 0xFFu);
    }
  }

  exi->state &= ~EXI_STATE_BUSY;
}

EXICallback EXISetExiCallback(s32 channel, EXICallback callback) {
  if (channel < 0 || channel >= MAX_CHAN) {
    return 0;
  }
  EXICallback prev = s_ecb[channel].exi_cb;
  s_ecb[channel].exi_cb = callback;
  return prev;
}

void EXIInit(void) {
  memset(gc_exi_regs, 0, sizeof(gc_exi_regs));
  memset(s_ecb, 0, sizeof(s_ecb));
}

BOOL EXILock(s32 channel, u32 device, EXICallback callback) {
  if (channel < 0 || channel >= MAX_CHAN) {
    return FALSE;
  }

  GcExiControl* exi = &s_ecb[channel];
  if (exi->state & EXI_STATE_LOCKED) {
    return FALSE;
  }

  exi->dev = device;
  exi->exi_cb = callback;
  exi->state |= EXI_STATE_LOCKED;
  return TRUE;
}

BOOL EXIUnlock(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) {
    return FALSE;
  }
  GcExiControl* exi = &s_ecb[channel];

  // Unlock while busy is a logic error in the SDK; keep deterministic behavior.
  if (exi->state & EXI_STATE_BUSY) {
    return FALSE;
  }

  exi->state &= ~EXI_STATE_LOCKED;
  return TRUE;
}

BOOL EXISelect(s32 channel, u32 device, u32 frequency) {
  (void)frequency;
  if (channel < 0 || channel >= MAX_CHAN) {
    return FALSE;
  }
  GcExiControl* exi = &s_ecb[channel];
  if ((exi->state & EXI_STATE_LOCKED) == 0) {
    return FALSE;
  }
  if (exi->state & EXI_STATE_SELECTED) {
    return FALSE;
  }

  exi->dev = device;
  exi->state |= EXI_STATE_SELECTED;

  // Model: reflect selected+device in STAT (best-effort). Real hardware packs bits here.
  *regp(channel, 0) = (exi->dev & 0x3u) | 0x100u;
  return TRUE;
}

BOOL EXIDeselect(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) {
    return FALSE;
  }
  GcExiControl* exi = &s_ecb[channel];
  if ((exi->state & EXI_STATE_SELECTED) == 0) {
    return FALSE;
  }
  if (exi->state & EXI_STATE_BUSY) {
    return FALSE;
  }

  exi->state &= ~EXI_STATE_SELECTED;
  return TRUE;
}

BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, EXICallback callback) {
  if (channel < 0 || channel >= MAX_CHAN) {
    return FALSE;
  }
  if (length <= 0 || length > 4) {
    return FALSE;
  }

  GcExiControl* exi = &s_ecb[channel];
  if ((exi->state & EXI_STATE_SELECTED) == 0) {
    return FALSE;
  }
  if (exi->state & EXI_STATE_BUSY) {
    return FALSE;
  }

  exi->tc_cb = callback;
  exi->imm_len = (int)length;
  exi->imm_buf = (uint8_t*)buffer;

  // Pack immediate write data into IMM register (MSB-first).
  if (type != EXI_READ) {
    const uint8_t* b = (const uint8_t*)buffer;
    u32 data = 0;
    for (s32 i = 0; i < length; i++) {
      data |= (u32)b[i] << ((3 - (u32)i) * 8);
    }
    *regp(channel, 4) = data;
  }

  exi->state |= EXI_STATE_IMM;
  // CONTROL: tstart=1, dma=0, rw=type, tlen=len-1.
  *regp(channel, 3) = exi_0cr(1u, 0u, type ? 1u : 0u, (u32)length - 1u);
  return TRUE;
}

BOOL EXIImmEx(s32 channel, void* buffer, s32 length, u32 type) {
  // Minimal: chunk into <=4 bytes and sync after each.
  uint8_t* p = (uint8_t*)buffer;
  s32 remaining = length;
  while (remaining > 0) {
    s32 n = remaining > 4 ? 4 : remaining;
    if (!EXIImm(channel, p, n, type, 0)) {
      return FALSE;
    }
    if (!EXISync(channel)) {
      return FALSE;
    }
    p += n;
    remaining -= n;
  }
  return TRUE;
}

BOOL EXIDma(s32 channel, void* buffer, s32 length, u32 type, EXICallback callback) {
  (void)buffer;
  (void)length;
  (void)type;
  (void)callback;
  // Not modeled yet. CARD will drive this; keep deterministic failure for now.
  return FALSE;
}

BOOL EXISync(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) {
    return FALSE;
  }
  GcExiControl* exi = &s_ecb[channel];
  if ((exi->state & EXI_STATE_BUSY) == 0 && (exi->state & EXI_STATE_IMM) == 0) {
    // In our model, Sync without a pending transfer is OK.
    return TRUE;
  }

  // In the real SDK this waits for transfer complete interrupt.
  // For deterministic host tests, we complete immediately.
  complete_transfer(channel);
  exi->state &= ~EXI_STATE_IMM;
  exi->imm_len = 0;
  exi->imm_buf = 0;
  return TRUE;
}

BOOL EXIProbe(s32 channel) {
  (void)channel;
  return FALSE;
}

s32 EXIProbeEx(s32 channel) {
  (void)channel;
  return 0;
}

BOOL EXIAttach(s32 channel, EXICallback callback) {
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  s_ecb[channel].ext_cb = callback;
  s_ecb[channel].state |= EXI_STATE_ATTACHED;
  return TRUE;
}

BOOL EXIDetach(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  s_ecb[channel].state &= ~EXI_STATE_ATTACHED;
  s_ecb[channel].ext_cb = 0;
  return TRUE;
}

u32 EXIGetState(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return 0;
  return s_ecb[channel].state;
}

s32 EXIGetID(s32 channel, u32 device, u32* id) {
  (void)device;
  if (channel < 0 || channel >= MAX_CHAN) return 0;
  if (id) *id = 0;
  return 0;
}

void EXIProbeReset(void) {}
