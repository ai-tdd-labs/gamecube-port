#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Use the sdk_port implementation directly.
uint32_t OSRoundUp32B(uint32_t x);
uint32_t OSRoundDown32B(uint32_t x);

// Simple deterministic PRNG (xorshift32)
static uint32_t xs32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void fail_u32(const char *name, uint32_t x, uint32_t got) {
    fprintf(stderr, "PBT FAIL: %s(x=0x%08X) got=0x%08X\n", name, x, got);
    exit(1);
}

int main(int argc, char **argv) {
    uint32_t iters = 200000;
    uint32_t seed = 0xC0DEC0DEu;
    if (argc >= 2) iters = (uint32_t)strtoul(argv[1], 0, 0);
    if (argc >= 3) seed = (uint32_t)strtoul(argv[2], 0, 0);

    for (uint32_t i = 0; i < iters; i++) {
        uint32_t x = xs32(&seed);

        uint32_t up = OSRoundUp32B(x);
        if ((up & 31u) != 0) fail_u32("OSRoundUp32B alignment", x, up);
        if (up < x) fail_u32("OSRoundUp32B monotonic", x, up);
        if (OSRoundUp32B(up) != up) fail_u32("OSRoundUp32B idempotent", x, up);

        uint32_t down = OSRoundDown32B(x);
        if ((down & 31u) != 0) fail_u32("OSRoundDown32B alignment", x, down);
        if (down > x) fail_u32("OSRoundDown32B monotonic", x, down);
        if (OSRoundDown32B(down) != down) fail_u32("OSRoundDown32B idempotent", x, down);

        // Consistency: down <= x <= up, and (up-down) is either 0 (already aligned) or 32.
        if (!(down <= x && x <= up)) {
            fprintf(stderr, "PBT FAIL: bounds x=0x%08X down=0x%08X up=0x%08X\n", x, down, up);
            return 1;
        }
        uint32_t gap = up - down;
        if (!(gap == 0u || gap == 32u)) {
            fprintf(stderr, "PBT FAIL: gap x=0x%08X down=0x%08X up=0x%08X\n", x, down, up);
            return 1;
        }
    }

    printf("PBT PASS: %u iterations\n", iters);
    return 0;
}
