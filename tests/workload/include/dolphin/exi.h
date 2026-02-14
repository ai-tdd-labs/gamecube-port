#pragma once

#include "dolphin/types.h"

// Minimal host-safe EXI header surface.
// Only includes the pieces needed to compile sdk_port + host scenarios.

typedef struct OSContext OSContext;

typedef void (*EXICallback)(s32 chan, OSContext* context);

// From OSExpansion.h / SDK conventions.
#define EXI_READ 0
#define EXI_WRITE 1

#define EXI_STATE_IDLE 0x00
#define EXI_STATE_DMA 0x01
#define EXI_STATE_IMM 0x02
#define EXI_STATE_BUSY (EXI_STATE_DMA | EXI_STATE_IMM)
#define EXI_STATE_SELECTED 0x04
#define EXI_STATE_ATTACHED 0x08
#define EXI_STATE_LOCKED 0x10

EXICallback EXISetExiCallback(s32 channel, EXICallback callback);

void EXIInit(void);
BOOL EXILock(s32 channel, u32 device, EXICallback callback);
BOOL EXIUnlock(s32 channel);
BOOL EXISelect(s32 channel, u32 device, u32 frequency);
BOOL EXIDeselect(s32 channel);
BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, EXICallback callback);
BOOL EXIImmEx(s32 channel, void* buffer, s32 length, u32 type);
BOOL EXIDma(s32 channel, void* buffer, s32 length, u32 type, EXICallback callback);
BOOL EXISync(s32 channel);
BOOL EXIProbe(s32 channel);
s32 EXIProbeEx(s32 channel);
BOOL EXIAttach(s32 channel, EXICallback callback);
BOOL EXIDetach(s32 channel);
u32 EXIGetState(s32 channel);
s32 EXIGetID(s32 channel, u32 device, u32* id);
void EXIProbeReset(void);

// Host-test knobs (sdk_port EXI model).
// These are intentionally simple globals so host scenarios can configure EXI
// presence/ID deterministically.
extern s32 gc_exi_probeex_ret[3]; /* -1 no device, 0 busy, >0 present */
extern u32 gc_exi_getid_ok[3];    /* 0 => EXIGetID fails, 1 => succeeds */
extern u32 gc_exi_id[3];          /* returned by EXIGetID when enabled */
extern u32 gc_exi_attach_ok[3];   /* 0 => EXIAttach fails, 1 => succeeds */

// Optional DMA hook for device-backed transfers (e.g. CARD).
// If NULL, EXIDma returns FALSE.
extern int (*gc_exi_dma_hook)(s32 channel, u32 exi_addr, void* buffer, s32 length, u32 type);
