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

// Host-test configuration knobs.
//
// These are not part of the real hardware; they're deterministic controls so
// higher-level subsystems (CARD) can be validated without needing a full EXI bus
// emulator upfront.
//
// Conventions:
// - EXIProbeEx returns:
//     -1 => no device
//      0 => busy
//     >0 => device present
// - EXIGetID returns 1 on success, 0 on failure (SDK convention).
s32 gc_exi_probeex_ret[MAX_CHAN] = { -1, -1, -1 };
u32 gc_exi_getid_ok[MAX_CHAN];
u32 gc_exi_id[MAX_CHAN];
u32 gc_exi_attach_ok[MAX_CHAN];

// Minimal CARD device status register model used by __CARDReadStatus/__CARDClearStatus.
// This is a host-test knob (real hardware status bits are device-defined).
u32 gc_exi_card_status[MAX_CHAN];
u32 gc_exi_card_status_reads[MAX_CHAN];
u32 gc_exi_card_status_clears[MAX_CHAN];

// Instrumentation: last immediate transfer per channel (MSB-first packed into u32).
u32 gc_exi_last_imm_len[MAX_CHAN];
u32 gc_exi_last_imm_type[MAX_CHAN];
u32 gc_exi_last_imm_data[MAX_CHAN];

// Instrumentation counters.
u32 gc_exi_deselect_calls[MAX_CHAN];
u32 gc_exi_unlock_calls[MAX_CHAN];

// Instrumentation: current callbacks (observable for deterministic unit tests).
uintptr_t gc_exi_exi_callback_ptr[MAX_CHAN];
uintptr_t gc_exi_ext_callback_ptr[MAX_CHAN];

// Optional DMA hook used to model device-backed transfers (e.g. CARD).
// If NULL, EXIDma returns FALSE.
//
// Contract: return 1 on success, 0 on failure.
int (*gc_exi_dma_hook)(s32 channel, u32 exi_addr, void* buffer, s32 length, u32 type);

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

typedef struct GcExiCardProto {
  uint8_t cmd0;
  u32 addr;
  int has_addr;
  int pending_status_read;
} GcExiCardProto;

static GcExiCardProto s_card[MAX_CHAN];

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
  gc_exi_exi_callback_ptr[channel] = (uintptr_t)callback;
  return prev;
}

void EXIInit(void) {
  memset(gc_exi_regs, 0, sizeof(gc_exi_regs));
  memset(s_ecb, 0, sizeof(s_ecb));
  memset(s_card, 0, sizeof(s_card));
  gc_exi_dma_hook = 0;
  // Default: no devices present.
  for (int i = 0; i < MAX_CHAN; i++) {
    gc_exi_probeex_ret[i] = -1;
    gc_exi_getid_ok[i] = 0;
    gc_exi_id[i] = 0;
    gc_exi_attach_ok[i] = 1;
    gc_exi_card_status[i] = 0;
    gc_exi_card_status_reads[i] = 0;
    gc_exi_card_status_clears[i] = 0;
    gc_exi_last_imm_len[i] = 0;
    gc_exi_last_imm_type[i] = 0;
    gc_exi_last_imm_data[i] = 0;
    gc_exi_deselect_calls[i] = 0;
    gc_exi_unlock_calls[i] = 0;
    gc_exi_exi_callback_ptr[i] = 0;
    gc_exi_ext_callback_ptr[i] = 0;
  }
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
  gc_exi_unlock_calls[channel]++;
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
  gc_exi_deselect_calls[channel]++;
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

  gc_exi_last_imm_len[channel] = (u32)length;
  gc_exi_last_imm_type[channel] = type;
  gc_exi_last_imm_data[channel] = 0;

  // Device->host immediate reads for a minimal CARD status register.
  if (type == EXI_READ) {
    if (length == 1 && exi->dev == 0 && s_card[channel].pending_status_read) {
      s_card[channel].pending_status_read = 0;
      gc_exi_card_status_reads[channel]++;
      // complete_transfer() copies from IMM MSB-first.
      *regp(channel, 4) = (gc_exi_card_status[channel] & 0xFFu) << 24;
      gc_exi_last_imm_data[channel] = *regp(channel, 4);
    }
  }

  // Pack immediate write data into IMM register (MSB-first).
  if (type != EXI_READ) {
    const uint8_t* b = (const uint8_t*)buffer;
    u32 data = 0;
    for (s32 i = 0; i < length; i++) {
      data |= (u32)b[i] << ((3 - (u32)i) * 8);
    }
    *regp(channel, 4) = data;
    gc_exi_last_imm_data[channel] = data;

    // Minimal CARD status command decode:
    // - 0x83: read status (next 1-byte EXI_READ returns status)
    // - 0x89: clear status
    if (exi->dev == 0 && length >= 1 && buffer) {
      uint8_t cmd = b[0];
      if (cmd == 0x83u) {
        s_card[channel].pending_status_read = 1;
      } else if (cmd == 0x89u) {
        s_card[channel].pending_status_read = 0;
        // Inferred from MP4 SDK usage: status bits 0x18 are treated as error flags
        // (decomp_mario_party_4/src/dolphin/card/CARDBios.c). Preserve other bits
        // like 0x40 (unlock indicator) so DoMount can observe it after clear+read.
        gc_exi_card_status[channel] &= ~0x18u;
        gc_exi_card_status_clears[channel]++;
      }
    }
  }

  exi->state |= EXI_STATE_IMM;
  // CONTROL: tstart=1, dma=0, rw=type, tlen=len-1.
  *regp(channel, 3) = exi_0cr(1u, 0u, type ? 1u : 0u, (u32)length - 1u);
  return TRUE;
}

BOOL EXIImmEx(s32 channel, void* buffer, s32 length, u32 type) {
  // Capture CARD-style command/address writes (5-byte command packets).
  if (channel >= 0 && channel < MAX_CHAN && type != EXI_READ && length >= 5 && buffer) {
    const uint8_t* b = (const uint8_t*)buffer;
    // Known CARD command encodings (from decomp CARDBios.c):
    // - 0x52: read segment
    // - 0xF2: write page
    // Address decode in SDK uses:
    //   addr = (cmd1<<17) | (cmd2<<9) | (cmd3<<7) | cmd4  (with masking)
    if (b[0] == 0x52 || b[0] == 0xF2 || b[0] == 0xF1) {
      s_card[channel].cmd0 = b[0];
      s_card[channel].addr =
          (((u32)(b[1] & 0x7Fu)) << 17) |
          (((u32)b[2]) << 9) |
          (((u32)(b[3] & 0x03u)) << 7) |
          ((u32)(b[4] & 0x7Fu));
      s_card[channel].has_addr = 1;
    }
  }

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
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  if (!buffer || length <= 0) return FALSE;

  GcExiControl* exi = &s_ecb[channel];
  if ((exi->state & EXI_STATE_SELECTED) == 0) return FALSE;
  if (exi->state & EXI_STATE_BUSY) return FALSE;

  // We model DMA as an immediate completion that optionally consults a device hook.
  if (!s_card[channel].has_addr) return FALSE;
  if (!gc_exi_dma_hook) return FALSE;

  exi->tc_cb = callback;
  exi->state |= EXI_STATE_DMA;
  // CONTROL: tstart=1, dma=1, rw=type, tlen ignored (best-effort).
  *regp(channel, 3) = exi_0cr(1u, 1u, type ? 1u : 0u, 0u);

  int ok = gc_exi_dma_hook(channel, s_card[channel].addr, buffer, length, type);
  exi->state &= ~EXI_STATE_DMA;
  if (!ok) return FALSE;

  // Deliver callback immediately (deterministic host model).
  if (exi->tc_cb) {
    EXICallback cb = exi->tc_cb;
    exi->tc_cb = 0;
    cb(channel, 0);
  }
  return TRUE;
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
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  return gc_exi_probeex_ret[channel] > 0;
}

s32 EXIProbeEx(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return -1;
  return gc_exi_probeex_ret[channel];
}

BOOL EXIAttach(s32 channel, EXICallback callback) {
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  if (!gc_exi_attach_ok[channel]) return FALSE;
  s_ecb[channel].ext_cb = callback;
  gc_exi_ext_callback_ptr[channel] = (uintptr_t)callback;
  s_ecb[channel].state |= EXI_STATE_ATTACHED;
  return TRUE;
}

BOOL EXIDetach(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return FALSE;
  s_ecb[channel].state &= ~EXI_STATE_ATTACHED;
  s_ecb[channel].ext_cb = 0;
  gc_exi_ext_callback_ptr[channel] = 0;
  return TRUE;
}

u32 EXIGetState(s32 channel) {
  if (channel < 0 || channel >= MAX_CHAN) return 0;
  return s_ecb[channel].state;
}

s32 EXIGetID(s32 channel, u32 device, u32* id) {
  (void)device;
  if (channel < 0 || channel >= MAX_CHAN) return 0;
  if (!gc_exi_getid_ok[channel]) {
    if (id) *id = 0;
    return 0;
  }
  if (id) *id = gc_exi_id[channel];
  return 1;
}

void EXIProbeReset(void) {}
