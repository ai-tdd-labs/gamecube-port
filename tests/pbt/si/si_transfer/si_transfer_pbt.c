#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../../src/sdk_port/gc_mem.h"
#include "../../../../src/sdk_port/sdk_state.h"

typedef int32_t s32;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int BOOL;

BOOL SITransfer(s32 chan, void *output, u32 outputBytes, void *input, u32 inputBytes, void *callback, u64 delay);
void gc_os_set_system_time_seed(u64 system_time);
void gc_os_set_setalarm_delta(u32 delta_ticks);
void gc_si_set_hw_xfer_ok(u32 ok);

static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void store_u32be(uint32_t addr, uint32_t v) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return;
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v >> 0);
}

static uint32_t load_u32be(uint32_t addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return 0;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void store_u64be(uint32_t addr, uint64_t v) {
    store_u32be(addr + 0u, (uint32_t)(v >> 32));
    store_u32be(addr + 4u, (uint32_t)(v & 0xFFFFFFFFu));
}

static uint64_t load_u64be(uint32_t addr) {
    return ((uint64_t)load_u32be(addr + 0u) << 32) | (uint64_t)load_u32be(addr + 4u);
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    enum {
        SI_PACKET_BASE = 0x801A6F98u,
        SI_PACKET_SIZE = 0x20u,
        SI_ALARM_BASE = 0x801A7018u,
        SI_ALARM_SIZE = 0x28u,
        SI_XFER_TIME_BASE = 0x801A70D8u,
        SI_ALARM_HANDLER_PC = 0x800D9C38u,
        OS_ALARM_QUEUE_PTR = 0x801A5E68u,
    };

    static uint8_t mem1[0x01800000];
    gc_mem_set(0x80000000u, sizeof(mem1), mem1);
    gc_sdk_state_reset();

    for (uint32_t i = 0; i < iters; i++) {
        s32 chan = (s32)(xs32(&seed) % 6u) - 1; // -1..4 (includes invalid)
        u32 out_ptr = 0x80300000u + (xs32(&seed) & 0x0000FFF0u);
        u32 in_ptr = 0x80400000u + (xs32(&seed) & 0x0000FFF0u);
        u32 out_len = xs32(&seed) % 1024u;
        u32 in_len = xs32(&seed) % 1024u;
        u32 cb_ptr = 0x80500000u + (xs32(&seed) & 0x0000FFF0u);
        u64 now = (((u64)xs32(&seed)) << 32) | (u64)xs32(&seed);
        u64 delay = (((u64)xs32(&seed) & 0xFFFFu) << 16) | (u64)(xs32(&seed) & 0xFFFFu);
        u64 xfer = (((u64)xs32(&seed)) << 32) | (u64)xs32(&seed);
        u32 alarm_delta = xs32(&seed) & 0x3FFu;
        u32 hw_ok = xs32(&seed) & 1u;

        gc_os_set_system_time_seed(now);
        gc_os_set_setalarm_delta(alarm_delta);
        gc_si_set_hw_xfer_ok(hw_ok);

        // Seed all xfer times; only current channel matters.
        for (u32 c = 0; c < 4u; c++) {
            store_u64be(SI_XFER_TIME_BASE + c * 8u, xfer + c);
        }

        // Sentinel packet/alarm for target channel.
        if (chan >= 0 && chan < 4) {
            u32 pkt = SI_PACKET_BASE + (u32)chan * SI_PACKET_SIZE;
            u32 alarm = SI_ALARM_BASE + (u32)chan * SI_ALARM_SIZE;
            for (u32 off = 0; off < SI_PACKET_SIZE; off += 4u) store_u32be(pkt + off, 0xDEADBEEFu);
            for (u32 off = 0; off < SI_ALARM_SIZE; off += 4u) store_u32be(alarm + off, 0xCAFEBABEu);
        }

        BOOL got = SITransfer(chan, (void *)(uintptr_t)out_ptr, out_len, (void *)(uintptr_t)in_ptr, in_len,
                              (void *)(uintptr_t)cb_ptr, delay);

        if (chan < 0 || chan >= 4) {
            if (got != 0) {
                fprintf(stderr, "PBT FAIL step=%u: invalid chan should fail\n", i);
                return 1;
            }
            continue;
        }

        u64 fire = (delay == 0u) ? now : (load_u64be(SI_XFER_TIME_BASE + (u32)chan * 8u) + delay);
        int fast_path = (now >= fire) && (hw_ok != 0u);
        if (got != 1) {
            fprintf(stderr, "PBT FAIL step=%u: valid chan should return true\n", i);
            return 1;
        }

        u32 pkt = SI_PACKET_BASE + (u32)chan * SI_PACKET_SIZE;
        u32 alarm = SI_ALARM_BASE + (u32)chan * SI_ALARM_SIZE;
        if (fast_path) {
            // Must return before packet/alarm writes.
            if (load_u32be(pkt + 0x00u) != 0xDEADBEEFu || load_u32be(alarm + 0x00u) != 0xCAFEBABEu) {
                fprintf(stderr, "PBT FAIL step=%u: fast-path wrote packet/alarm unexpectedly\n", i);
                return 1;
            }
            continue;
        }

        if (load_u32be(pkt + 0x00u) != (u32)chan) {
            fprintf(stderr, "PBT FAIL step=%u: packet chan mismatch\n", i);
            return 1;
        }
        if (load_u32be(pkt + 0x04u) != out_ptr || load_u32be(pkt + 0x08u) != out_len ||
            load_u32be(pkt + 0x0Cu) != in_ptr || load_u32be(pkt + 0x10u) != in_len ||
            load_u32be(pkt + 0x14u) != cb_ptr || load_u64be(pkt + 0x18u) != fire) {
            fprintf(stderr, "PBT FAIL step=%u: packet fields mismatch\n", i);
            return 1;
        }

        if (now < fire) {
            if (load_u32be(alarm + 0x00u) != SI_ALARM_HANDLER_PC ||
                load_u64be(alarm + 0x08u) != (fire + (u64)alarm_delta) ||
                load_u32be(alarm + 0x14u) != OS_ALARM_QUEUE_PTR) {
                fprintf(stderr, "PBT FAIL step=%u: alarm fields mismatch\n", i);
                return 1;
            }
        } else {
            // Non-fast path with now>=fire only happens when hw transfer fails;
            // alarm must stay untouched.
            if (load_u32be(alarm + 0x00u) != 0xCAFEBABEu) {
                fprintf(stderr, "PBT FAIL step=%u: alarm unexpectedly written\n", i);
                return 1;
            }
        }
    }

    printf("PBT PASS: si_transfer %u iterations\n", iters);
    return 0;
}
