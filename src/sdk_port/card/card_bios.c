#include <stdint.h>
#include <string.h>
#include "card_bios.h"
#include "dolphin/exi.h"
#include "dolphin/os.h"
#include "card_dir.h"
#include "card_mem.h"

/* CARD results (mp4-decomp dolphin/card.h). */
enum {
    CARD_RESULT_READY = 0,
    CARD_RESULT_NOCARD = -3,
    CARD_RESULT_NOFILE = -4,
    CARD_RESULT_BROKEN = -6,
    CARD_RESULT_NOPERM = -10,
    CARD_RESULT_BUSY = -1,
    CARD_RESULT_FATAL_ERROR = -128,
};

GcCardControl gc_card_block[GC_CARD_CHANS];

uint32_t gc_card_dsp_init_calls;
uint32_t gc_card_os_init_alarm_calls;
uint32_t gc_card_os_register_reset_calls;
uint32_t gc_card_tx_calls[GC_CARD_CHANS];

// Forward decls (used by internal callbacks).
s32 __CARDReadSegment(s32 chan, CARDCallback callback);

static void DSPInit(void) { gc_card_dsp_init_calls++; }
static void OSInitAlarm(void) { gc_card_os_init_alarm_calls++; }

extern uint16_t OSGetFontEncode(void);

static void OSInitThreadQueue(uint32_t *dst, int chan)
{
    /* Modeled: mark initialized with a stable cookie. */
    *dst = 0x51554555u ^ (uint32_t)chan;
}

static void OSCreateAlarm(uint32_t *dst, int chan)
{
    /* Modeled: mark created with a stable cookie. */
    *dst = 0x414C524Du ^ (uint32_t)chan;
}

static void *OSPhysicalToCached(uint32_t paddr)
{
    /* On GC, physical MEM1 0 maps to 0x80000000 cached. */
    (void)paddr;
    return (void *)(uintptr_t)0x80000000u;
}

static void OSRegisterResetFunction(void *info)
{
    (void)info;
    gc_card_os_register_reset_calls++;
}

static void __CARDSetDiskID(uintptr_t id)
{
    /* Modeled: only the passed pointer value is observable for our suites. */
    gc_card_block[0].disk_id = id;
    gc_card_block[1].disk_id = id;
}

void __CARDSyncCallback(s32 chan, s32 result)
{
    (void)result;
    OSWakeupThread((OSThreadQueue *)&gc_card_block[chan].thread_queue_inited);
}

void __CARDDefaultApiCallback(s32 chan, s32 result)
{
    (void)chan;
    (void)result;
}

static inline int card_compare_filename(const u8 *entry_addr, const char *file_name)
{
    const char *n = file_name;
    int i;

    for (i = 0; i < (int)PORT_CARD_FILENAME_MAX; i++) {
        char a = (char)entry_addr[PORT_CARD_DIR_OFF_FILENAME + (u32)i];
        char b = n[i];
        if (a != b) {
            return 0;
        }
        if (a == '\0') {
            return 1;
        }
    }

    return file_name[PORT_CARD_FILENAME_MAX] == '\0';
}

s32 __CARDAccess(const GcCardControl* card, const void* dirEntry)
{
    const u8 *entry = (const u8 *)dirEntry;
    if (!card || !entry) {
        return -128; // CARD_RESULT_FATAL_ERROR
    }
    if (entry[PORT_CARD_DIR_OFF_GAMENAME] == 0xFFu) {
        return CARD_RESULT_NOFILE;
    }
    if (memcmp(entry + PORT_CARD_DIR_OFF_GAMENAME, card->id, 4) == 0 &&
        memcmp(entry + PORT_CARD_DIR_OFF_COMPANY, card->id + 4, 2) == 0) {
        return CARD_RESULT_READY;
    }
    return CARD_RESULT_NOPERM;
}

s32 __CARDGetFileNo(const GcCardControl* card, const char* fileName, s32* pfileNo)
{
    if (!card || !fileName || !pfileNo) {
        return -128; // CARD_RESULT_FATAL_ERROR
    }
    if (!card->attached || !card->current_dir_ptr) {
        return CARD_RESULT_NOCARD;
    }

    for (s32 fileNo = 0; fileNo < PORT_CARD_MAX_FILE; fileNo++) {
        const u8 *entry = (const u8 *)card_ptr_ro(card->current_dir_ptr + (u32)fileNo * PORT_CARD_DIR_SIZE,
                                                  PORT_CARD_DIR_SIZE);
        if (!entry) {
            continue;
        }
        s32 access = __CARDAccess(card, entry);
        if (access < 0) {
            continue;
        }
        if (card_compare_filename(entry, fileName)) {
            *pfileNo = fileNo;
            return CARD_RESULT_READY;
        }
    }
    return CARD_RESULT_NOFILE;
}

s32 __CARDIsOpened(const GcCardControl* card, s32 fileNo)
{
    (void)card;
    (void)fileNo;
    return FALSE;
}

void CARDInit(void)
{
    int chan;

    if (gc_card_block[0].disk_id && gc_card_block[1].disk_id) {
        return;
    }

    DSPInit();
    OSInitAlarm();

    for (chan = 0; chan < GC_CARD_CHANS; ++chan) {
        GcCardControl *card = &gc_card_block[chan];
        card->result = CARD_RESULT_NOCARD;
        OSInitThreadQueue(&card->thread_queue_inited, chan);
        OSCreateAlarm(&card->alarm_created, chan);
    }

    __CARDSetDiskID((uintptr_t)OSPhysicalToCached(0));
    OSRegisterResetFunction((void *)0);
}

s32 __CARDReadStatus(s32 chan, u8 *status)
{
    BOOL err;
    u8 cmd[2];

    if (!EXISelect(chan, 0, 4)) {
        return CARD_RESULT_NOCARD;
    }

    // Decomp (MP4 SDK): cmd = 0x83000000 sent as 2 bytes.
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

s32 __CARDClearStatus(s32 chan)
{
    BOOL err;
    u8 cmd;

    if (!EXISelect(chan, 0, 4)) {
        return CARD_RESULT_NOCARD;
    }

    // Decomp (MP4 SDK): cmd = 0x89000000 sent as 1 byte.
    cmd = 0x89;

    err = FALSE;
    err |= !EXIImm(chan, &cmd, 1, EXI_WRITE, 0);
    err |= !EXISync(chan);
    err |= !EXIDeselect(chan);

    return err ? CARD_RESULT_NOCARD : CARD_RESULT_READY;
}

s32 __CARDEnableInterrupt(s32 chan, BOOL enable)
{
    BOOL err;
    u8 cmd[2];

    if (!EXISelect(chan, 0, 4)) {
        return CARD_RESULT_NOCARD;
    }

    // Decomp (MP4 SDK): cmd = enable ? 0x81010000 : 0x81000000, sent as 2 bytes.
    // Use explicit bytes so host endianness does not affect the transfer.
    cmd[0] = 0x81;
    cmd[1] = enable ? 0x01 : 0x00;

    err = FALSE;
    err |= !EXIImm(chan, cmd, 2, EXI_WRITE, 0);
    err |= !EXISync(chan);
    err |= !EXIDeselect(chan);
    return err ? CARD_RESULT_NOCARD : CARD_RESULT_READY;
}

s32 __CARDPutControlBlock(GcCardControl *card, s32 result)
{
    int enabled;

    // Minimal port: replicate CARDBios.c semantics for attached/busy updates.
    enabled = OSDisableInterrupts();
    if (card->attached) {
        card->result = result;
    }
    else if (card->result == -1) { // CARD_RESULT_BUSY
        card->result = result;
    }
    OSRestoreInterrupts(enabled);
    return result;
}

s32 __CARDGetControlBlock(s32 chan, GcCardControl **pcard)
{
    int enabled;
    s32 result;
    GcCardControl *card;

    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return CARD_RESULT_FATAL_ERROR;
    }

    card = &gc_card_block[chan];
    if (card->disk_id == 0) {
        return CARD_RESULT_FATAL_ERROR;
    }

    enabled = OSDisableInterrupts();
    if (!card->attached) {
        result = CARD_RESULT_NOCARD;
    } else if (card->result == CARD_RESULT_BUSY) {
        result = CARD_RESULT_BUSY;
    } else {
        card->result = CARD_RESULT_BUSY;
        card->api_callback = 0;
        if (pcard) {
            *pcard = card;
        }
        result = CARD_RESULT_READY;
    }
    OSRestoreInterrupts(enabled);
    return result;
}

s32 CARDGetResultCode(s32 chan)
{
    if (chan < 0 || chan >= 2) {
        return -128; // CARD_RESULT_FATAL_ERROR
    }
    return gc_card_block[chan].result;
}

s32 __CARDSync(s32 chan)
{
    s32 result;
    int enabled;
    GcCardControl *block;

    if (chan < 0 || chan >= 2) {
        return -128;
    }

    block = &gc_card_block[chan];
    enabled = OSDisableInterrupts();
    while ((result = CARDGetResultCode(chan)) == -1) { // CARD_RESULT_BUSY
        // Our host port does not model real OSThreadQueue objects for CARD.
        // Use the address of an internal per-channel cookie as a stable queue key.
        OSSleepThread((OSThreadQueue *)&block->thread_queue_inited);
    }
    OSRestoreInterrupts(enabled);
    return result;
}

u16 __CARDGetFontEncode(void)
{
    return OSGetFontEncode();
}

static void __CARDUnlockedHandler(s32 chan, OSContext *context)
{
    (void)context;
    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return;
    }
    GcCardControl *card = &gc_card_block[chan];
    if (card->unlock_callback) {
        CARDCallback cb = (CARDCallback)(uintptr_t)card->unlock_callback;
        card->unlock_callback = 0;
        cb(chan, EXIProbe(chan) ? CARD_RESULT_READY : CARD_RESULT_NOCARD);
    }
}

static s32 __CARDStart(s32 chan, CARDCallback txCallback, CARDCallback exiCallback)
{
    int enabled;
    GcCardControl *card;
    s32 result;

    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return -128; // CARD_RESULT_FATAL_ERROR
    }

    enabled = OSDisableInterrupts();
    card = &gc_card_block[chan];
    if (!card->attached) {
        result = CARD_RESULT_NOCARD;
    } else {
        if (txCallback) {
            card->tx_callback = (uintptr_t)txCallback;
        }
        if (exiCallback) {
            card->exi_callback = (uintptr_t)exiCallback;
        }

        // Decomp uses an "unlocked handler" callback while waiting for EXILock.
        // In our host model EXILock is synchronous; we keep the same call shape
        // because tests exercise the BUSY lock path.
        card->unlock_callback = 0;
        if (!EXILock(chan, 0, __CARDUnlockedHandler)) {
            result = CARD_RESULT_BUSY;
        } else {
            card->unlock_callback = 0;
            if (!EXISelect(chan, 0, 4)) {
                EXIUnlock(chan);
                result = CARD_RESULT_NOCARD;
            } else {
                // SetupTimeoutAlarm(card) not modeled yet.
                result = CARD_RESULT_READY;
            }
        }
    }
    OSRestoreInterrupts(enabled);
    return result;
}

void __CARDTxHandler(s32 chan, OSContext *context)
{
    (void)context;
    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return;
    }

    GcCardControl *card = &gc_card_block[chan];
    CARDCallback callback;
    BOOL err;

    err = !EXIDeselect(chan);
    EXIUnlock(chan);

    // Host-port note:
    // The real SDK observes status via EXI interrupts and routes completion via
    // txCallback (reads) or exiCallback (writes/erases). Our deterministic EXI
    // model completes DMA immediately, so dispatch based on the last command.
    callback = 0;
    if (card->cmd[0] == 0x52u) { // read segment
        callback = (CARDCallback)(uintptr_t)card->tx_callback;
        card->tx_callback = 0;
    }
    else {
        callback = (CARDCallback)(uintptr_t)card->exi_callback;
        card->exi_callback = 0;
    }

    if (callback) {
        gc_card_tx_calls[chan]++;
        callback(chan, (!err && EXIProbe(chan)) ? CARD_RESULT_READY : CARD_RESULT_NOCARD);
    }
}

static void BlockReadCallback(s32 chan, s32 result)
{
    GcCardControl *card;
    CARDCallback callback;

    card = &gc_card_block[chan];
    if (result < 0) {
        goto error;
    }

    card->xferred += 512;
    card->addr += 512;
    card->buffer += 512;
    if (--card->repeat <= 0) {
        goto error;
    }

    result = __CARDReadSegment(chan, BlockReadCallback);
    if (result < 0) {
        goto error;
    }
    return;

error:
    if (card->api_callback == 0) {
        __CARDPutControlBlock(card, result);
    }
    callback = (CARDCallback)(uintptr_t)card->xfer_callback;
    if (callback) {
        card->xfer_callback = 0;
        callback(chan, result);
    }
}

#define AD1(x) ((u8)(((x) >> 17) & 0x7f))
#define AD2(x) ((u8)(((x) >> 9) & 0xff))
#define AD3(x) ((u8)(((x) >> 7) & 0x03))
#define BA(x)  ((u8)((x)&0x7f))

enum { CARDID_SIZE = 512 };

s32 __CARDReadSegment(s32 chan, CARDCallback callback)
{
    GcCardControl *card;
    s32 result;

    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return -128; // CARD_RESULT_FATAL_ERROR
    }

    card = &gc_card_block[chan];
    card->cmd[0] = 0x52;
    card->cmd[1] = AD1(card->addr);
    card->cmd[2] = AD2(card->addr);
    card->cmd[3] = AD3(card->addr);
    card->cmd[4] = BA(card->addr);
    card->cmdlen = 5;
    card->mode = 0;
    card->retry = 0;

    result = __CARDStart(chan, callback, 0);
    if (result == CARD_RESULT_BUSY) {
        return CARD_RESULT_READY;
    } else if (result >= 0) {
        if (!EXIImmEx(chan, card->cmd, card->cmdlen, EXI_WRITE) ||
            !EXIImmEx(chan, (u8 *)(uintptr_t)card->work_area + CARDID_SIZE, (s32)card->latency, EXI_WRITE) ||
            !EXIDma(chan, (void *)(uintptr_t)card->buffer, 512, card->mode, __CARDTxHandler)) {
            card->tx_callback = 0;
            EXIDeselect(chan);
            EXIUnlock(chan);
            result = CARD_RESULT_NOCARD;
        } else {
            result = CARD_RESULT_READY;
        }
    }
    return result;
}

s32 __CARDRead(s32 chan, u32 addr, s32 length, void *dst, CARDCallback callback)
{
    GcCardControl *card;
    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return -128; // CARD_RESULT_FATAL_ERROR
    }
    card = &gc_card_block[chan];
    if (!card->attached) {
        return CARD_RESULT_NOCARD;
    }

    card->xfer_callback = (uintptr_t)callback;
    card->repeat = (int)(length / 512);
    card->addr = addr;
    card->buffer = (uintptr_t)dst;
    return __CARDReadSegment(chan, BlockReadCallback);
}

s32 __CARDWritePage(s32 chan, CARDCallback callback)
{
    GcCardControl *card;
    s32 result;

    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return -128;
    }

    card = &gc_card_block[chan];
    if (!card->attached) {
        return -3;
    }

    card->cmd[0] = 0xF2u;
    card->cmd[1] = AD1(card->addr);
    card->cmd[2] = AD2(card->addr);
    card->cmd[3] = AD3(card->addr);
    card->cmd[4] = BA(card->addr);
    card->cmdlen = 5;
    card->mode = 1;
    card->retry = 3;

    result = __CARDStart(chan, 0, callback);
    if (result == -1) {
        return 0;
    }
    if (result >= 0) {
        if (!EXIImmEx(chan, card->cmd, card->cmdlen, EXI_WRITE) ||
            !EXIDma(chan, (void *)(uintptr_t)card->buffer, 128, card->mode, __CARDTxHandler)) {
            card->exi_callback = 0;
            EXIDeselect(chan);
            EXIUnlock(chan);
            result = -3;
        }
        else {
            result = 0;
        }
    }
    return result;
}

s32 __CARDEraseSector(s32 chan, u32 addr, CARDCallback callback)
{
    GcCardControl *card;
    s32 result;

    if (chan < 0 || chan >= GC_CARD_CHANS) {
        return -128;
    }

    card = &gc_card_block[chan];
    if (!card->attached) {
        return -3;
    }

    card->cmd[0] = 0xF1u;
    card->cmd[1] = AD1(addr);
    card->cmd[2] = AD2(addr);
    card->cmdlen = 3;
    card->mode = -1;
    card->retry = 3;
    result = __CARDStart(chan, 0, callback);

    if (result == -1) {
        return 0;
    }
    else if (result >= 0) {
        if (!EXIImmEx(chan, card->cmd, card->cmdlen, EXI_WRITE)) {
            card->exi_callback = 0;
            result = -3;
        }
        else {
            result = 0;
        }

        EXIDeselect(chan);
        EXIUnlock(chan);
    }
    // Deterministic host model: treat erase as immediate completion (no EXI IRQ).
    if (result >= 0 && callback) {
        CARDCallback cb = callback;
        card->exi_callback = 0;
        cb(chan, CARD_RESULT_READY);
    }
    return result;
}
