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

const char *gc_scenario_label(void) { return "OSLink/rvz_trace_replay_001"; }

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
        snprintf(path, sizeof(path), "../actual/os_link_rvz_trace_%s.bin", safe);
    } else {
        snprintf(path, sizeof(path), "../actual/os_link_rvz_trace_replay_001.bin");
    }
    return path;
}

void gc_scenario_run(GcRam *ram) {
    (void)ram;
    const char *env_expect_ret = getenv("GC_EXPECT_RET");
    const char *env_expect_prev_nz = getenv("GC_EXPECT_PREV_NONZERO");
    const char *env_expect_next_nz = getenv("GC_EXPECT_NEXT_NONZERO");
    const char *env_expect_head_nz = getenv("GC_EXPECT_HEAD_NONZERO");
    const char *env_expect_tail_nz = getenv("GC_EXPECT_TAIL_NONZERO");
    const char *env_seed_tail = getenv("GC_SEED_HAS_TAIL");
    if (!env_expect_ret || !*env_expect_ret) die("GC_EXPECT_RET is required");
    if (!env_expect_prev_nz || !*env_expect_prev_nz) die("GC_EXPECT_PREV_NONZERO is required");
    if (!env_expect_next_nz || !*env_expect_next_nz) die("GC_EXPECT_NEXT_NONZERO is required");
    if (!env_expect_head_nz || !*env_expect_head_nz) die("GC_EXPECT_HEAD_NONZERO is required");
    if (!env_expect_tail_nz || !*env_expect_tail_nz) die("GC_EXPECT_TAIL_NONZERO is required");
    if (!env_seed_tail || !*env_seed_tail) die("GC_SEED_HAS_TAIL is required");

    int expect_ret = (int)strtoul(env_expect_ret, 0, 0);
    int expect_prev_nz = (int)strtoul(env_expect_prev_nz, 0, 0);
    int expect_next_nz = (int)strtoul(env_expect_next_nz, 0, 0);
    int expect_head_nz = (int)strtoul(env_expect_head_nz, 0, 0);
    int expect_tail_nz = (int)strtoul(env_expect_tail_nz, 0, 0);
    int seed_has_tail = (int)strtoul(env_seed_tail, 0, 0);

    OSModuleInfo module;
    memset(&module, 0, sizeof(module));
    module.id = 1;
    module.numSections = 1;
    module.version = 2;

    OSModuleInfo seed_tail;
    memset(&seed_tail, 0, sizeof(seed_tail));
    seed_tail.id = 0xFEEDu;

    __OSModuleInfoList.head = 0;
    __OSModuleInfoList.tail = 0;
    if (seed_has_tail) {
        __OSModuleInfoList.head = &seed_tail;
        __OSModuleInfoList.tail = &seed_tail;
    }

    int actual_ret = OSLink(&module, 0);

    int actual_prev_nz = module.link.prev ? 1 : 0;
    int actual_next_nz = module.link.next ? 1 : 0;
    int actual_head_nz = __OSModuleInfoList.head ? 1 : 0;
    int actual_tail_nz = __OSModuleInfoList.tail ? 1 : 0;

    int ok = 1;
    ok = ok && (actual_ret == expect_ret);
    ok = ok && (actual_prev_nz == expect_prev_nz);
    ok = ok && (actual_next_nz == expect_next_nz);
    ok = ok && (actual_head_nz == expect_head_nz);
    ok = ok && (actual_tail_nz == expect_tail_nz);

    store_u32be(0x80300000u, ok ? 0xDEADBEEFu : 0x0BADF00Du);
    store_u32be(0x80300004u, (uint32_t)expect_ret);
    store_u32be(0x80300008u, (uint32_t)actual_ret);
    store_u32be(0x8030000Cu, (uint32_t)expect_prev_nz);
    store_u32be(0x80300010u, (uint32_t)actual_prev_nz);
    store_u32be(0x80300014u, (uint32_t)expect_next_nz);
    store_u32be(0x80300018u, (uint32_t)actual_next_nz);
    store_u32be(0x8030001Cu, (uint32_t)expect_head_nz);
    store_u32be(0x80300020u, (uint32_t)actual_head_nz);
    store_u32be(0x80300024u, (uint32_t)expect_tail_nz);
    store_u32be(0x80300028u, (uint32_t)actual_tail_nz);
}
