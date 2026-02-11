#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../workload/include/dolphin/os.h"

extern OSModuleQueue __OSModuleInfoList;

BOOL OSLink(OSModuleInfo *newModule, void *bss);
BOOL OSUnlink(OSModuleInfo *oldModule);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void fail_step(uint32_t i, const char *msg) {
    fprintf(stderr, "PBT FAIL step=%u: %s\n", i, msg);
    exit(1);
}

static int model_find(const int *order, int len, int idx) {
    for (int i = 0; i < len; i++) {
        if (order[i] == idx) return i;
    }
    return -1;
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    enum { NODE_COUNT = 16 };
    OSModuleInfo nodes[NODE_COUNT];
    memset(nodes, 0, sizeof(nodes));
    for (int i = 0; i < NODE_COUNT; i++) {
        nodes[i].id = (uint32_t)(0x1000 + i);
    }

    __OSModuleInfoList.head = 0;
    __OSModuleInfoList.tail = 0;

    int order[NODE_COUNT];
    int order_len = 0;

    for (uint32_t i = 0; i < iters; i++) {
        int idx = (int)(xs32(&seed) % NODE_COUNT);
        int op = (int)(xs32(&seed) % 2u); // 0=link, 1=unlink
        int pos = model_find(order, order_len, idx);
        int in_model = (pos >= 0);
        int exp_ret = 0;

        if (op == 0) {
            // OSLink: TRUE for non-null, idempotent for already-linked.
            exp_ret = 1;
            if (!in_model) {
                order[order_len++] = idx;
            }
            BOOL got = OSLink(&nodes[idx], 0);
            if ((int)got != exp_ret) fail_step(i, "OSLink return mismatch");
        } else {
            // OSUnlink: FALSE for unknown/unlinked node.
            exp_ret = in_model ? 1 : 0;
            BOOL got = OSUnlink(&nodes[idx]);
            if ((int)got != exp_ret) fail_step(i, "OSUnlink return mismatch");
            if (in_model) {
                for (int j = pos; j + 1 < order_len; j++) order[j] = order[j + 1];
                order_len--;
            }
        }

        // Validate head/tail presence against model.
        if ((order_len == 0) != (__OSModuleInfoList.head == 0)) fail_step(i, "head empty mismatch");
        if ((order_len == 0) != (__OSModuleInfoList.tail == 0)) fail_step(i, "tail empty mismatch");

        // Walk queue and compare exact order.
        OSModuleInfo *cur = __OSModuleInfoList.head;
        int walk = 0;
        OSModuleInfo *prev = 0;
        while (cur) {
            if (walk >= NODE_COUNT) fail_step(i, "cycle detected");
            int got_idx = (int)(cur - nodes);
            if (walk >= order_len || got_idx != order[walk]) fail_step(i, "queue order mismatch");
            if (cur->link.prev != prev) fail_step(i, "prev pointer mismatch");
            prev = cur;
            cur = cur->link.next;
            walk++;
        }
        if (walk != order_len) fail_step(i, "queue length mismatch");
        if (order_len > 0) {
            OSModuleInfo *tail_exp = &nodes[order[order_len - 1]];
            if (__OSModuleInfoList.tail != tail_exp) fail_step(i, "tail pointer mismatch");
            if (__OSModuleInfoList.tail->link.next != 0) fail_step(i, "tail next not null");
        }

        // Validate each node pointer state.
        for (int n = 0; n < NODE_COUNT; n++) {
            int npos = model_find(order, order_len, n);
            if (npos < 0) {
                if (nodes[n].link.prev || nodes[n].link.next) fail_step(i, "unlinked node has dangling links");
            } else {
                OSModuleInfo *exp_prev = (npos > 0) ? &nodes[order[npos - 1]] : 0;
                OSModuleInfo *exp_next = (npos + 1 < order_len) ? &nodes[order[npos + 1]] : 0;
                if (nodes[n].link.prev != exp_prev) fail_step(i, "linked node prev mismatch");
                if (nodes[n].link.next != exp_next) fail_step(i, "linked node next mismatch");
            }
        }
    }

    printf("PBT PASS: os_module_queue %u iterations\n", iters);
    return 0;
}
