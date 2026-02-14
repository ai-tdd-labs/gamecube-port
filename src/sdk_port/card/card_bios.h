#pragma once

#include <stdint.h>

/* Minimal modeled state for CARD init/mount workflows.
 * This does not attempt to match the real SDK struct layout.
 */

enum { GC_CARD_CHANS = 2 };

typedef void (*CARDCallback)(int32_t chan, int32_t result);

typedef struct GcCardControl {
    int32_t result;
    uint32_t thread_queue_inited;
    uint32_t alarm_created;
    uintptr_t disk_id;

    // Additional modeled state used by CARDProbeEx/CARDMount chains.
    // Keep these appended so existing tests that only access the earlier fields
    // remain valid (offset-stable).
    int32_t attached;
    int32_t mount_step;
    int32_t size_mb;      // logical size (id & 0xFC) as used by CARDProbeEx
    int32_t sector_size;  // bytes

    // CARDMountAsync preflight state (modeled).
    uintptr_t work_area;
    uintptr_t ext_callback;
    uintptr_t api_callback;
    uintptr_t exi_callback;
    uintptr_t unlock_callback;
    uint32_t  alarm_cancel_calls;
    int32_t   current_dir;
    int32_t   current_fat;

    // DoMount step0 modeled fields (subset of real CARDControl).
    uint32_t  cid;
    uint32_t  cblock;
    uint32_t  latency;
    uint8_t   id[12];

    // __CARDRead/__CARDReadSegment state (subset of real CARDControl).
    // Keep appended for offset stability across existing suites.
    uint8_t   cmd[9];
    int32_t   cmdlen;
    uint32_t  mode;
    int32_t   retry;
    int32_t   repeat;
    uint32_t  addr;
    uintptr_t buffer;
    int32_t   xferred;
    uintptr_t tx_callback;
    uintptr_t xfer_callback;

    // __CARDVerify state (subset of real CARDControl).
    uint16_t  size_u16; // CARDControl.size used by CARDCheck.c (ID validation)
    uint16_t  _pad_verify0;
    uintptr_t current_dir_ptr;
    uintptr_t current_fat_ptr;
} GcCardControl;

/* CARDFileInfo (host-side, mirrors decomp layout). */
typedef struct {
    int32_t chan;
    int32_t fileNo;
    int32_t offset;
    int32_t length;
    uint16_t iBlock;
    uint16_t __padding;
} CARDFileInfo;

/* CARDStat mirror for CARDGetStatus parity work. */
enum {
    CARD_FILENAME_MAX = 32,
    CARD_ICON_MAX = 8,

    CARD_READ_SIZE = 512,
    CARD_COMMENT_SIZE = 64,
    CARD_ICON_WIDTH = 32,
    CARD_ICON_HEIGHT = 32,
    CARD_BANNER_WIDTH = 96,
    CARD_BANNER_HEIGHT = 32,

    CARD_STAT_ICON_NONE = 0,
    CARD_STAT_ICON_C8 = 1,
    CARD_STAT_ICON_RGB5A3 = 2,
    CARD_STAT_ICON_MASK = 3,

    CARD_STAT_BANNER_NONE = 0,
    CARD_STAT_BANNER_C8 = 1,
    CARD_STAT_BANNER_RGB5A3 = 2,
    CARD_STAT_BANNER_MASK = 3,

    CARD_STAT_SPEED_FAST = 1,
    CARD_STAT_SPEED_MIDDLE = 2,
    CARD_STAT_SPEED_SLOW = 3,
    CARD_STAT_SPEED_MASK = 3,
};

typedef struct {
    char fileName[CARD_FILENAME_MAX];
    uint32_t length;
    uint32_t time;
    uint8_t gameName[4];
    uint8_t company[2];

    uint8_t bannerFormat;
    uint8_t __padding;
    uint32_t iconAddr;
    uint16_t iconFormat;
    uint16_t iconSpeed;
    uint32_t commentAddr;

    uint32_t offsetBanner;
    uint32_t offsetBannerTlut;
    uint32_t offsetIcon[CARD_ICON_MAX];
    uint32_t offsetIconTlut;
    uint32_t offsetData;
} CARDStat;

extern GcCardControl gc_card_block[GC_CARD_CHANS];

/* Observable side-effect counters for CARDInit. */
extern uint32_t gc_card_dsp_init_calls;
extern uint32_t gc_card_os_init_alarm_calls;
extern uint32_t gc_card_os_register_reset_calls;
extern uint32_t gc_card_tx_calls[GC_CARD_CHANS];

void CARDInit(void);

// Internal CARDBios helpers (modeled subset).
int32_t __CARDGetControlBlock(int32_t chan, GcCardControl** pcard);
int32_t __CARDSync(int32_t chan);
void __CARDSyncCallback(int32_t chan, int32_t result);

/* CARD open / close API (decomp: external/mp4-decomp/src/dolphin/card/CARDOpen.c). */
int32_t CARDOpen(int32_t chan, const char* fileName, CARDFileInfo* fileInfo);
int32_t CARDClose(CARDFileInfo* fileInfo);

/* CARD filesystem metric API (decomp: external/mp4-decomp/src/dolphin/card/CARDBios.c). */
int32_t CARDFreeBlocks(int32_t chan, int32_t* byteNotUsed, int32_t* filesNotUsed);
int32_t CARDGetSectorSize(int32_t chan, uint32_t* size);

/* CARD serial API (decomp: external/mp4-decomp/src/dolphin/card/CARDNet.c). */
int32_t CARDGetSerialNo(int32_t chan, uint64_t* serialNo);

/* CARD status API (decomp: external/mp4-decomp/src/dolphin/card/CARDStat.c). */
int32_t CARDGetStatus(int32_t chan, int32_t fileNo, CARDStat* stat);
