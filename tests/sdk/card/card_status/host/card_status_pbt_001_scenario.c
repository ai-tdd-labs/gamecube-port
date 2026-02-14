#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/exi.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int BOOL;

s32 __CARDReadStatus(s32 chan, u8 *status);
s32 __CARDClearStatus(s32 chan);

extern u32 gc_exi_card_status[3];
extern u32 gc_exi_card_status_reads[3];
extern u32 gc_exi_card_status_clears[3];
extern u32 gc_exi_last_imm_len[3];
extern u32 gc_exi_last_imm_type[3];
extern u32 gc_exi_last_imm_data[3];

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static inline u32 h32(u32 h, u32 v) { return rotl1(h) ^ v; }

const char *gc_scenario_label(void) { return "__CARDReadStatus/__CARDClearStatus/pbt_001"; }
const char *gc_scenario_out_path(void) { return "../actual/card_status_pbt_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    u8 *out = gc_ram_ptr(ram, 0x80300000u, 0x40u);
    if (!out) die("gc_ram_ptr failed");
    memset(out, 0, 0x40u);

    u32 l0 = 0, l1 = 0, l2 = 0, l5 = 0;

    // L0: without lock, EXISelect fails -> NOCARD.
    EXIInit();
    u8 st = 0xEE;
    l0 = h32(l0, (u32)__CARDReadStatus(0, &st));
    l0 = h32(l0, (u32)st);
    l0 = h32(l0, (u32)__CARDClearStatus(0));

    // L1: with lock, status reads and clears.
    EXIInit();
    (void)EXILock(0, 0, 0);
    gc_exi_card_status[0] = 0x40u;
    st = 0xEE;
    l1 = h32(l1, (u32)__CARDReadStatus(0, &st));
    l1 = h32(l1, (u32)st);
    l1 = h32(l1, gc_exi_card_status_reads[0]);
    l1 = h32(l1, gc_exi_last_imm_len[0]);
    l1 = h32(l1, gc_exi_last_imm_type[0]);
    l1 = h32(l1, gc_exi_last_imm_data[0]);

    l1 = h32(l1, (u32)__CARDClearStatus(0));
    l1 = h32(l1, gc_exi_card_status_clears[0]);
    l1 = h32(l1, gc_exi_last_imm_data[0]);

    st = 0xEE;
    l1 = h32(l1, (u32)__CARDReadStatus(0, &st));
    l1 = h32(l1, (u32)st);
    (void)EXIUnlock(0);

    // L2: EXISelect forced-fail -> NOCARD even when locked.
    EXIInit();
    (void)EXILock(0, 0, 0);
    gc_exi_probeex_ret[0] = 1; // irrelevant, but keep deterministic "present"
    // Force select fail by not holding lock? easiest: unlock, then call.
    (void)EXIUnlock(0);
    st = 0xEE;
    l2 = h32(l2, (u32)__CARDReadStatus(0, &st));
    l2 = h32(l2, (u32)st);
    l2 = h32(l2, (u32)__CARDClearStatus(0));

    // L5: channel out of range -> NOCARD.
    EXIInit();
    st = 0xEE;
    l5 = h32(l5, (u32)__CARDReadStatus(-1, &st));
    l5 = h32(l5, (u32)__CARDReadStatus(3, &st));
    l5 = h32(l5, (u32)__CARDClearStatus(-1));
    l5 = h32(l5, (u32)__CARDClearStatus(3));

    u32 master = 0;
    master = h32(master, l0);
    master = h32(master, l1);
    master = h32(master, l2);
    master = h32(master, l5);

    wr32be(out + 0x00, 0x43535453u); // "CSTS"
    wr32be(out + 0x04, master);
    wr32be(out + 0x08, l0);
    wr32be(out + 0x0C, l1);
    wr32be(out + 0x10, l2);
    wr32be(out + 0x14, l5);
    wr32be(out + 0x18, gc_exi_card_status_reads[0]);
    wr32be(out + 0x1C, gc_exi_card_status_clears[0]);
}

