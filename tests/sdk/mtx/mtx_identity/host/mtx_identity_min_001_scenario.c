#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef float f32;
typedef f32 Mtx[3][4];

void C_MTXIdentity(Mtx mtx);

static u32 f2u(f32 f) {
    u32 u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

const char *gc_scenario_label(void) { return "C_MTXIdentity/min"; }
const char *gc_scenario_out_path(void) { return "../actual/mtx_identity_min_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    Mtx m;
    // Fill with noise to prove we overwrite deterministically.
    for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) m[r][c] = -123.0f;

    C_MTXIdentity(m);

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x40);
    if (!p) die("gc_ram_ptr failed");

    wr32be(p + 0x00, 0xDEADBEEFu);
    wr32be(p + 0x04, f2u(m[0][0]));
    wr32be(p + 0x08, f2u(m[0][1]));
    wr32be(p + 0x0C, f2u(m[0][2]));
    wr32be(p + 0x10, f2u(m[0][3]));

    wr32be(p + 0x14, f2u(m[1][0]));
    wr32be(p + 0x18, f2u(m[1][1]));
    wr32be(p + 0x1C, f2u(m[1][2]));
    wr32be(p + 0x20, f2u(m[1][3]));

    wr32be(p + 0x24, f2u(m[2][0]));
    wr32be(p + 0x28, f2u(m[2][1]));
    wr32be(p + 0x2C, f2u(m[2][2]));
    wr32be(p + 0x30, f2u(m[2][3]));
}

