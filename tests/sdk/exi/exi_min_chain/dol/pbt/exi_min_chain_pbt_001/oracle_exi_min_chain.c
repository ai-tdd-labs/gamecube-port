#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

enum { FALSE = 0, TRUE = 1 };

// Mirror minimal constants from OSExpansion.h.
enum {
    EXI_READ = 0,
    EXI_WRITE = 1,
    EXI_STATE_DMA = 0x01,
    EXI_STATE_IMM = 0x02,
    EXI_STATE_BUSY = (EXI_STATE_DMA | EXI_STATE_IMM),
    EXI_STATE_SELECTED = 0x04,
    EXI_STATE_ATTACHED = 0x08,
    EXI_STATE_LOCKED = 0x10,
};

typedef struct OSContext OSContext;
typedef void (*EXICallback)(s32 chan, OSContext* context);

enum { MAX_CHAN = 3, REG_MAX = 5 };

u32 gc_exi_regs[16];

typedef struct ExiCtl {
    EXICallback exi_cb;
    EXICallback tc_cb;
    EXICallback ext_cb;
    u32 state;
    int imm_len;
    uint8_t* imm_buf;
    u32 dev;
} ExiCtl;

static ExiCtl s_ecb[MAX_CHAN];

static inline volatile u32* regp(s32 chan, int idx) {
    return &gc_exi_regs[(u32)chan * REG_MAX + (u32)idx];
}

static inline u32 exi_0cr(u32 tstart, u32 dma, u32 rw, u32 tlen) {
    return ((tstart & 1u) << 0) | ((dma & 1u) << 1) | ((rw & 1u) << 2) | ((tlen & 0xFu) << 4);
}

static void complete_transfer(s32 chan) {
    ExiCtl* exi = &s_ecb[chan];
    if ((exi->state & EXI_STATE_BUSY) == 0) return;

    if ((exi->state & EXI_STATE_IMM) && exi->imm_len > 0 && exi->imm_buf) {
        u32 data = *regp(chan, 4);
        for (int i = 0; i < exi->imm_len; i++) {
            exi->imm_buf[i] = (uint8_t)((data >> ((3 - i) * 8)) & 0xFFu);
        }
    }
    exi->state &= ~EXI_STATE_BUSY;
}

void oracle_EXIReset(void) {
    memset(gc_exi_regs, 0, sizeof(gc_exi_regs));
    memset(s_ecb, 0, sizeof(s_ecb));
}

BOOL EXILock(s32 channel, u32 device, EXICallback callback) {
    if (channel < 0 || channel >= MAX_CHAN) return FALSE;
    ExiCtl* exi = &s_ecb[channel];
    if (exi->state & EXI_STATE_LOCKED) return FALSE;
    exi->dev = device;
    exi->exi_cb = callback;
    exi->state |= EXI_STATE_LOCKED;
    return TRUE;
}

BOOL EXIUnlock(s32 channel) {
    if (channel < 0 || channel >= MAX_CHAN) return FALSE;
    ExiCtl* exi = &s_ecb[channel];
    if (exi->state & EXI_STATE_BUSY) return FALSE;
    exi->state &= ~EXI_STATE_LOCKED;
    return TRUE;
}

BOOL EXISelect(s32 channel, u32 device, u32 frequency) {
    (void)frequency;
    if (channel < 0 || channel >= MAX_CHAN) return FALSE;
    ExiCtl* exi = &s_ecb[channel];
    if ((exi->state & EXI_STATE_LOCKED) == 0) return FALSE;
    if (exi->state & EXI_STATE_SELECTED) return FALSE;
    exi->dev = device;
    exi->state |= EXI_STATE_SELECTED;
    *regp(channel, 0) = (exi->dev & 0x3u) | 0x100u;
    return TRUE;
}

BOOL EXIDeselect(s32 channel) {
    if (channel < 0 || channel >= MAX_CHAN) return FALSE;
    ExiCtl* exi = &s_ecb[channel];
    if ((exi->state & EXI_STATE_SELECTED) == 0) return FALSE;
    if (exi->state & EXI_STATE_BUSY) return FALSE;
    exi->state &= ~EXI_STATE_SELECTED;
    return TRUE;
}

BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, EXICallback callback) {
    (void)callback;
    if (channel < 0 || channel >= MAX_CHAN) return FALSE;
    if (length <= 0 || length > 4) return FALSE;
    ExiCtl* exi = &s_ecb[channel];
    if ((exi->state & EXI_STATE_SELECTED) == 0) return FALSE;
    if (exi->state & EXI_STATE_BUSY) return FALSE;

    exi->tc_cb = callback;
    exi->imm_len = (int)length;
    exi->imm_buf = (uint8_t*)buffer;

    if (type != EXI_READ) {
        const uint8_t* b = (const uint8_t*)buffer;
        u32 data = 0;
        for (s32 i = 0; i < length; i++) {
            data |= (u32)b[i] << ((3 - (u32)i) * 8);
        }
        *regp(channel, 4) = data;
    }

    exi->state |= EXI_STATE_IMM;
    *regp(channel, 3) = exi_0cr(1u, 0u, type ? 1u : 0u, (u32)length - 1u);
    return TRUE;
}

BOOL EXISync(s32 channel) {
    if (channel < 0 || channel >= MAX_CHAN) return FALSE;
    ExiCtl* exi = &s_ecb[channel];
    if ((exi->state & EXI_STATE_BUSY) == 0 && (exi->state & EXI_STATE_IMM) == 0) return TRUE;
    complete_transfer(channel);
    exi->state &= ~EXI_STATE_IMM;
    exi->imm_len = 0;
    exi->imm_buf = 0;
    return TRUE;
}

u32 EXIGetState(s32 channel) {
    if (channel < 0 || channel >= MAX_CHAN) return 0;
    return s_ecb[channel].state;
}

