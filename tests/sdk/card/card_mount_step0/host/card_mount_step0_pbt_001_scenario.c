#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/exi.h"
#include "dolphin/OSRtcPriv.h"

#include "sdk_port/card/card_bios.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

enum {
  CARD_RESULT_READY = 0,
  CARD_RESULT_NOCARD = -3,
  CARD_RESULT_IOERROR = -5,
};

typedef void (*CARDCallback)(s32 chan, s32 result);

s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback);

// Deterministic unlock stub knobs (sdk_port).
extern u32 gc_card_unlock_ok[2];
extern u8  gc_card_unlock_flash_id[2][12];
extern u32 gc_card_unlock_calls[2];

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline u32 h32(u32 h, u32 v) { return rotl1(h) ^ v; }

static u32 hash_card(u32 h, int chan) {
    h = h32(h, (u32)gc_card_block[chan].result);
    h = h32(h, (u32)gc_card_block[chan].attached);
    h = h32(h, (u32)gc_card_block[chan].mount_step);
    h = h32(h, (u32)gc_card_block[chan].size_mb);
    h = h32(h, (u32)gc_card_block[chan].sector_size);
    h = h32(h, (u32)gc_card_block[chan].cid);
    h = h32(h, (u32)gc_card_block[chan].cblock);
    h = h32(h, (u32)gc_card_block[chan].latency);
    for (int i = 0; i < 12; i++) h = h32(h, (u32)gc_card_block[chan].id[i]);
    return h;
}

static void set_sram_flashid(int chan, const u8* id12) {
    OSSramEx* s = __OSLockSramEx();
    if (!s) die("__OSLockSramEx failed");
    u8 c = 0;
    for (int i = 0; i < 12; i++) {
        s->flashID[chan][i] = id12[i];
        c = (u8)(c + id12[i]);
    }
    s->flashIDCheckSum[chan] = (u8)~c;
    (void)__OSUnlockSramEx(1);
}

static u32 hash_sram(u32 h, int chan) {
    OSSramEx* s = __OSLockSramEx();
    if (!s) die("__OSLockSramEx failed");
    for (int i = 0; i < 12; i++) h = h32(h, (u32)s->flashID[chan][i]);
    h = h32(h, (u32)s->flashIDCheckSum[chan]);
    (void)__OSUnlockSramEx(0);
    return h;
}

const char* gc_scenario_label(void) { return "CARDMount/DoMount_step0/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/card_mount_step0_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
    u8* out = gc_ram_ptr(ram, 0x80300000u, 0x40u);
    if (!out) die("gc_ram_ptr failed");
    memset(out, 0, 0x40u);

    const u8 sram_id0[12] = { 1,2,3,4,5,6,7,8,9,10,11,12 };
    const u8 unlock_id0[12] = { 0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB };

    u32 l0 = 0, l1 = 0, l2 = 0;

    // Case 0: status 0x40, SRAM checksum OK.
    *gc_ram_ptr(ram, 0x800030E3u, 1) = 0;
    memset(gc_card_block, 0, sizeof(gc_card_block));
    EXIInit();
    __OSInitSram();
    gc_exi_probeex_ret[0] = 1;
    gc_exi_getid_ok[0] = 1;
    gc_exi_id[0] = 0x00000004u;
    gc_exi_card_status[0] = 0x40u;
    set_sram_flashid(0, sram_id0);
    s32 r0 = CARDMountAsync(0, (void*)0x1234, 0, 0);
    l0 = h32(l0, (u32)r0);
    l0 = hash_card(l0, 0);
    l0 = h32(l0, gc_exi_card_status_reads[0]);
    l0 = h32(l0, gc_exi_card_status_clears[0]);
    l0 = hash_sram(l0, 0);

    // Case 1: status 0x00 -> unlock path writes SRAM.
    *gc_ram_ptr(ram, 0x800030E3u, 1) = 0;
    memset(gc_card_block, 0, sizeof(gc_card_block));
    EXIInit();
    __OSInitSram();
    gc_exi_probeex_ret[0] = 1;
    gc_exi_getid_ok[0] = 1;
    gc_exi_id[0] = 0x00000004u;
    gc_exi_card_status[0] = 0x00u;
    memset(gc_card_unlock_ok, 0, sizeof(gc_card_unlock_ok));
    memset(gc_card_unlock_flash_id, 0, sizeof(gc_card_unlock_flash_id));
    memset(gc_card_unlock_calls, 0, sizeof(gc_card_unlock_calls));
    gc_card_unlock_ok[0] = 1;
    memcpy(gc_card_unlock_flash_id[0], unlock_id0, 12);
    s32 r1 = CARDMountAsync(0, (void*)0x1234, 0, 0);
    l1 = h32(l1, (u32)r1);
    l1 = hash_card(l1, 0);
    l1 = h32(l1, gc_card_unlock_calls[0]);
    l1 = hash_sram(l1, 0);

    // Case 2: status 0x40 but checksum mismatch -> IOERROR.
    *gc_ram_ptr(ram, 0x800030E3u, 1) = 0;
    memset(gc_card_block, 0, sizeof(gc_card_block));
    EXIInit();
    __OSInitSram();
    gc_exi_probeex_ret[0] = 1;
    gc_exi_getid_ok[0] = 1;
    gc_exi_id[0] = 0x00000004u;
    gc_exi_card_status[0] = 0x40u;
    set_sram_flashid(0, sram_id0);
    {
        OSSramEx* s = __OSLockSramEx();
        if (!s) die("__OSLockSramEx failed");
        s->flashIDCheckSum[0] ^= 0x01u;
        (void)__OSUnlockSramEx(0);
    }
    s32 r2 = CARDMountAsync(0, (void*)0x1234, 0, 0);
    l2 = h32(l2, (u32)r2);
    l2 = hash_card(l2, 0);

    u32 master = 0;
    master = h32(master, l0);
    master = h32(master, l1);
    master = h32(master, l2);

    wr32be(out + 0x00, 0x4D533030u); // "MS00"
    wr32be(out + 0x04, master);
    wr32be(out + 0x08, l0);
    wr32be(out + 0x0C, l1);
    wr32be(out + 0x10, l2);
}
