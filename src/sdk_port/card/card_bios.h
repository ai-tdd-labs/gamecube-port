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

extern GcCardControl gc_card_block[GC_CARD_CHANS];

/* Observable side-effect counters for CARDInit. */
extern uint32_t gc_card_dsp_init_calls;
extern uint32_t gc_card_os_init_alarm_calls;
extern uint32_t gc_card_os_register_reset_calls;
extern uint32_t gc_card_tx_calls[GC_CARD_CHANS];

void CARDInit(void);
