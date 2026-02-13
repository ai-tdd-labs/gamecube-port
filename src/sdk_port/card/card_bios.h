#pragma once

#include <stdint.h>

/* Minimal modeled state for CARD init/mount workflows.
 * This does not attempt to match the real SDK struct layout.
 */

enum { GC_CARD_CHANS = 2 };

typedef struct GcCardControl {
    int32_t result;
    uint32_t thread_queue_inited;
    uint32_t alarm_created;
    uintptr_t disk_id;
} GcCardControl;

extern GcCardControl gc_card_block[GC_CARD_CHANS];

/* Observable side-effect counters for CARDInit. */
extern uint32_t gc_card_dsp_init_calls;
extern uint32_t gc_card_os_init_alarm_calls;
extern uint32_t gc_card_os_register_reset_calls;

void CARDInit(void);

