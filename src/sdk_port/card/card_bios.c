#include <stdint.h>
#include "card_bios.h"

/* CARD results (mp4-decomp dolphin/card.h). */
enum { CARD_RESULT_NOCARD = -3 };

GcCardControl gc_card_block[GC_CARD_CHANS];

uint32_t gc_card_dsp_init_calls;
uint32_t gc_card_os_init_alarm_calls;
uint32_t gc_card_os_register_reset_calls;

static void DSPInit(void) { gc_card_dsp_init_calls++; }
static void OSInitAlarm(void) { gc_card_os_init_alarm_calls++; }

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
