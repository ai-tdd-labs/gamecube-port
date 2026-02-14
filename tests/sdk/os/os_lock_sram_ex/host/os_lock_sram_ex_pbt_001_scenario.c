#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

#include "dolphin/OSRtcPriv.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef int BOOL;

void __OSInitSram(void);
OSSram* __OSLockSram(void);
OSSramEx* __OSLockSramEx(void);
BOOL __OSUnlockSram(BOOL commit);
BOOL __OSUnlockSramEx(BOOL commit);
BOOL __OSSyncSram(void);

extern u32 gc_os_sram_read_ok;
extern u32 gc_os_sram_write_ok;
extern u32 gc_os_scb_locked;
extern u32 gc_os_scb_enabled;
extern u32 gc_os_scb_sync;
extern u32 gc_os_scb_offset;
extern u32 gc_os_sram_read_calls;
extern u32 gc_os_sram_write_calls;

static inline u32 rotl1(u32 v) { return (v << 1) | (v >> 31); }
static u32 hash_u32(u32 h, u32 v) { return rotl1(h) ^ v; }
static u32 hash_bytes(u32 h, const void* data, u32 n) {
    const u8* p = (const u8*)data;
    for (u32 i = 0; i < n; i++) h = rotl1(h) ^ p[i];
    return h;
}

static u32 snap_ex(u32 h) {
    OSSramEx* ex = __OSLockSramEx();
    h = hash_u32(h, (u32)(ex != NULL));
    if (ex) {
        h = hash_bytes(h, ex->flashID, (u32)sizeof(ex->flashID));
        h = hash_bytes(h, ex->flashIDCheckSum, (u32)sizeof(ex->flashIDCheckSum));
    }
    (void)__OSUnlockSramEx(0);
    return h;
}

const char* gc_scenario_label(void) { return "__OSLockSramEx/__OSUnlockSramEx/pbt_001"; }
const char* gc_scenario_out_path(void) { return "../actual/os_lock_sram_ex_pbt_001.bin"; }

void gc_scenario_run(GcRam* ram) {
    u8* out = gc_ram_ptr(ram, 0x80300000u, 0x60u);
    if (!out) die("gc_ram_ptr failed");
    memset(out, 0, 0x60u);

    // L0
    gc_os_sram_read_ok = 1;
    gc_os_sram_write_ok = 1;
    __OSInitSram();
    u32 l0 = 0;
    OSSramEx* ex1 = __OSLockSramEx();
    l0 = hash_u32(l0, (u32)(ex1 != NULL));
    OSSramEx* ex2 = __OSLockSramEx();
    l0 = hash_u32(l0, (u32)(ex2 == NULL));
    if (ex1) {
        for (u32 i = 0; i < 12; i++) ex1->flashID[0][i] = (u8)(0xA0u + i);
        ex1->flashIDCheckSum[0] = 0x5Au;
    }
    l0 = hash_u32(l0, (u32)__OSUnlockSramEx(0));
    l0 = snap_ex(l0);
    l0 = hash_u32(l0, (u32)__OSUnlockSramEx(1));
    OSSramEx* ex3 = __OSLockSramEx();
    l0 = hash_u32(l0, (u32)(ex3 != NULL));
    if (ex3) {
        l0 = hash_u32(l0, (u32)ex3->flashID[0][0]);
        l0 = hash_u32(l0, (u32)ex3->flashIDCheckSum[0]);
    }
    l0 = hash_u32(l0, (u32)__OSUnlockSramEx(1));

    // L1
    __OSInitSram();
    u32 l1 = 0;
    gc_os_sram_write_ok = 0;
    OSSramEx* exf = __OSLockSramEx();
    if (exf) exf->flashIDCheckSum[1] = 0x99u;
    l1 = hash_u32(l1, (u32)__OSUnlockSramEx(1));
    l1 = hash_u32(l1, gc_os_scb_sync);
    l1 = hash_u32(l1, gc_os_scb_offset);
    gc_os_sram_write_ok = 1;
    OSSramEx* exf2 = __OSLockSramEx();
    if (exf2) exf2->flashIDCheckSum[1] ^= 0x01u;
    l1 = hash_u32(l1, (u32)__OSUnlockSramEx(1));
    l1 = hash_u32(l1, gc_os_scb_sync);
    l1 = hash_u32(l1, gc_os_scb_offset);

    // L2
    __OSInitSram();
    u32 l2 = 0;
    OSSram* s0 = __OSLockSram();
    l2 = hash_u32(l2, (u32)(s0 != NULL));
    OSSramEx* ex_blocked = __OSLockSramEx();
    l2 = hash_u32(l2, (u32)(ex_blocked == NULL));
    if (s0) {
        s0->flags = 0x80u;
    }
    l2 = hash_u32(l2, (u32)__OSUnlockSram(1));
    OSSram* s1 = __OSLockSram();
    if (s1) {
        l2 = hash_u32(l2, (u32)s1->flags);
    }
    (void)__OSUnlockSram(0);

    // L5
    u32 l5 = 0;
    __OSInitSram();
    l5 = hash_u32(l5, (u32)__OSUnlockSramEx(0));
    l5 = hash_u32(l5, (u32)__OSUnlockSramEx(0));
    l5 = hash_u32(l5, gc_os_scb_locked);

    u32 master = 0;
    master = hash_u32(master, l0);
    master = hash_u32(master, l1);
    master = hash_u32(master, l2);
    master = hash_u32(master, l5);

    wr32be(out + 0x00, 0x4F534558u); // "OSEX"
    wr32be(out + 0x04, master);
    wr32be(out + 0x08, l0);
    wr32be(out + 0x0C, l1);
    wr32be(out + 0x10, l2);
    wr32be(out + 0x14, l5);

    wr32be(out + 0x18, gc_os_sram_read_calls);
    wr32be(out + 0x1C, gc_os_sram_write_calls);
    wr32be(out + 0x20, gc_os_scb_sync);
    wr32be(out + 0x24, gc_os_scb_offset);
    wr32be(out + 0x28, gc_os_scb_locked);
    wr32be(out + 0x2C, gc_os_scb_enabled);

    // Persisted flashID sample.
    OSSramEx* ex_final = __OSLockSramEx();
    if (ex_final) {
        memcpy(out + 0x30, ex_final->flashID[0], 12);
        out[0x3C] = ex_final->flashIDCheckSum[0];
        out[0x3D] = ex_final->flashIDCheckSum[1];
    }
    (void)__OSUnlockSramEx(0);

    (void)__OSLockSram();
    (void)__OSUnlockSram(0);
}
