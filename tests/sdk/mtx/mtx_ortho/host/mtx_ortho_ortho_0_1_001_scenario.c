#include <stdint.h>
#include <string.h>

#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef float f32;
typedef f32 Mtx44[4][4];

void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);

static u32 f2u(f32 f) {
    u32 u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

const char *gc_scenario_label(void) { return "C_MTXOrtho/ortho_0_1"; }
const char *gc_scenario_out_path(void) { return "../actual/mtx_ortho_ortho_0_1_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    Mtx44 m;
    // t=0,b=1,l=0,r=1,n=0,f=1
    C_MTXOrtho(m, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

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
    wr32be(p + 0x34, f2u(m[3][0]));
    wr32be(p + 0x38, f2u(m[3][1]));
    wr32be(p + 0x3C, f2u(m[3][2]));
}

