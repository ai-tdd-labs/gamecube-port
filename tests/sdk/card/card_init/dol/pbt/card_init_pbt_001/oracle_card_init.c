#include <stdint.h>

typedef uint32_t u32;
typedef int32_t s32;

enum { CARD_RESULT_NOCARD = -3 };

typedef struct GcCardControl {
    s32 result;
    u32 thread_queue_inited;
    u32 alarm_created;
    uintptr_t disk_id;
} GcCardControl;

GcCardControl gc_card_block[2];

u32 gc_card_dsp_init_calls;
u32 gc_card_os_init_alarm_calls;
u32 gc_card_os_register_reset_calls;

static void DSPInit(void) { gc_card_dsp_init_calls++; }
static void OSInitAlarm(void) { gc_card_os_init_alarm_calls++; }

static void OSInitThreadQueue(u32 *dst, int chan) { *dst = 0x51554555u ^ (u32)chan; }
static void OSCreateAlarm(u32 *dst, int chan) { *dst = 0x414C524Du ^ (u32)chan; }

static void *OSPhysicalToCached(u32 paddr) { (void)paddr; return (void *)(uintptr_t)0x80000000u; }

static void OSRegisterResetFunction(void *info) { (void)info; gc_card_os_register_reset_calls++; }

static void __CARDSetDiskID(uintptr_t id)
{
    gc_card_block[0].disk_id = id;
    gc_card_block[1].disk_id = id;
}

void oracle_CARDInit(void)
{
    int chan;

    if (gc_card_block[0].disk_id && gc_card_block[1].disk_id) {
        return;
    }

    DSPInit();
    OSInitAlarm();

    for (chan = 0; chan < 2; ++chan) {
        GcCardControl *card = &gc_card_block[chan];
        card->result = CARD_RESULT_NOCARD;
        OSInitThreadQueue(&card->thread_queue_inited, chan);
        OSCreateAlarm(&card->alarm_created, chan);
    }

    __CARDSetDiskID((uintptr_t)OSPhysicalToCached(0));
    OSRegisterResetFunction((void *)0);
}

