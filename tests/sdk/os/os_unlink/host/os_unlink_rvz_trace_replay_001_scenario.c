#include "gc_host_scenario.h"
#include "gc_mem.h"
#include "dolphin/os.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern OSModuleQueue __OSModuleInfoList;

static void die(const char *msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(2);
}

static void store_u32be(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

const char *gc_scenario_label(void) { return "OSUnlink/rvz_trace_replay_001"; }

const char *gc_scenario_out_path(void) {
    const char *case_id = getenv("GC_TRACE_CASE_ID");
    static char path[512];
    if (case_id && *case_id) {
        char safe[256];
        size_t j = 0;
        for (size_t i = 0; case_id[i] && j + 1 < sizeof(safe); i++) {
            char c = case_id[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_' || c == '-' || c == '.') {
                safe[j++] = c;
            }
        }
        safe[j] = 0;
        snprintf(path, sizeof(path), "../actual/os_unlink_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/os_unlink_rvz_trace_replay_001.bin");
    }
    return path;
}

static int req_i(const char *name) {
    const char *s = getenv(name);
    if (!s || !*s) {
        fprintf(stderr, "fatal: %s is required\n", name);
        exit(2);
    }
    return (int)strtoul(s, 0, 0);
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    int expect_ret = req_i("GC_EXPECT_RET");
    int in_prev_nz = req_i("GC_IN_PREV_NONZERO");
    int in_next_nz = req_i("GC_IN_NEXT_NONZERO");
    int in_head_nz = req_i("GC_IN_HEAD_NONZERO");
    int in_tail_nz = req_i("GC_IN_TAIL_NONZERO");
    int in_head_is_old = req_i("GC_IN_HEAD_IS_OLD");
    int in_tail_is_old = req_i("GC_IN_TAIL_IS_OLD");

    int out_prev_nz = req_i("GC_OUT_PREV_NONZERO");
    int out_next_nz = req_i("GC_OUT_NEXT_NONZERO");
    int out_head_nz = req_i("GC_OUT_HEAD_NONZERO");
    int out_tail_nz = req_i("GC_OUT_TAIL_NONZERO");
    int out_head_is_old = req_i("GC_OUT_HEAD_IS_OLD");
    int out_tail_is_old = req_i("GC_OUT_TAIL_IS_OLD");

    OSModuleInfo old_mod;
    OSModuleInfo prev_mod;
    OSModuleInfo next_mod;
    OSModuleInfo head_stub;
    OSModuleInfo tail_stub;
    memset(&old_mod, 0, sizeof(old_mod));
    memset(&prev_mod, 0, sizeof(prev_mod));
    memset(&next_mod, 0, sizeof(next_mod));
    memset(&head_stub, 0, sizeof(head_stub));
    memset(&tail_stub, 0, sizeof(tail_stub));

    if (in_prev_nz) {
        old_mod.link.prev = &prev_mod;
        prev_mod.link.next = &old_mod;
    }
    if (in_next_nz) {
        old_mod.link.next = &next_mod;
        next_mod.link.prev = &old_mod;
    }

    __OSModuleInfoList.head = 0;
    __OSModuleInfoList.tail = 0;

    if (in_head_nz) {
        if (in_head_is_old) {
            __OSModuleInfoList.head = &old_mod;
        } else if (in_prev_nz) {
            __OSModuleInfoList.head = &prev_mod;
        } else if (in_next_nz) {
            __OSModuleInfoList.head = &next_mod;
        } else {
            __OSModuleInfoList.head = &head_stub;
        }
    }

    if (in_tail_nz) {
        if (in_tail_is_old) {
            __OSModuleInfoList.tail = &old_mod;
        } else if (in_next_nz) {
            __OSModuleInfoList.tail = &next_mod;
        } else if (in_prev_nz) {
            __OSModuleInfoList.tail = &prev_mod;
        } else {
            __OSModuleInfoList.tail = &tail_stub;
        }
    }

    int actual_ret = OSUnlink(&old_mod);

    int actual_prev_nz = old_mod.link.prev ? 1 : 0;
    int actual_next_nz = old_mod.link.next ? 1 : 0;
    int actual_head_nz = __OSModuleInfoList.head ? 1 : 0;
    int actual_tail_nz = __OSModuleInfoList.tail ? 1 : 0;
    int actual_head_is_old = (__OSModuleInfoList.head == &old_mod) ? 1 : 0;
    int actual_tail_is_old = (__OSModuleInfoList.tail == &old_mod) ? 1 : 0;

    int ok = 1;
    ok = ok && (actual_ret == expect_ret);
    ok = ok && (actual_prev_nz == out_prev_nz);
    ok = ok && (actual_next_nz == out_next_nz);
    ok = ok && (actual_head_nz == out_head_nz);
    ok = ok && (actual_tail_nz == out_tail_nz);
    ok = ok && (actual_head_is_old == out_head_is_old);
    ok = ok && (actual_tail_is_old == out_tail_is_old);

    store_u32be(0x80300000u, ok ? 0xDEADBEEFu : 0x0BADF00Du);
    store_u32be(0x80300004u, (uint32_t)expect_ret);
    store_u32be(0x80300008u, (uint32_t)actual_ret);
    store_u32be(0x8030000Cu, (uint32_t)out_prev_nz);
    store_u32be(0x80300010u, (uint32_t)actual_prev_nz);
    store_u32be(0x80300014u, (uint32_t)out_next_nz);
    store_u32be(0x80300018u, (uint32_t)actual_next_nz);
    store_u32be(0x8030001Cu, (uint32_t)out_head_nz);
    store_u32be(0x80300020u, (uint32_t)actual_head_nz);
    store_u32be(0x80300024u, (uint32_t)out_tail_nz);
    store_u32be(0x80300028u, (uint32_t)actual_tail_nz);
    store_u32be(0x8030002Cu, (uint32_t)out_head_is_old);
    store_u32be(0x80300030u, (uint32_t)actual_head_is_old);
    store_u32be(0x80300034u, (uint32_t)out_tail_is_old);
    store_u32be(0x80300038u, (uint32_t)actual_tail_is_old);
}

