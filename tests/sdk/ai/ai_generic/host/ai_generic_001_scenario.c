#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef void (*AIDCallback)(void);

AIDCallback AIRegisterDMACallback(AIDCallback callback);
void AIInitDMA(u32 addr, u32 length);
void AIStartDMA(void);
u32  AIGetDMAStartAddr(void);
void AISetStreamPlayState(u32 state);
u32  AIGetStreamPlayState(void);
void AISetStreamVolLeft(u8 volume);
u8   AIGetStreamVolLeft(void);
void AISetStreamVolRight(u8 volume);
u8   AIGetStreamVolRight(void);
u32  AIGetStreamSampleRate(void);

extern u32 gc_ai_regs[4];
extern u16 gc_ai_dsp_regs[4];

static inline void wr16be_local(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v);
}

static void dummy_dma_cb(void) {}

const char *gc_scenario_label(void) { return "AI/generic"; }
const char *gc_scenario_out_path(void) { return "../actual/ai_generic_001.bin"; }

void gc_scenario_run(GcRam *ram) {
    (void)ram;

    uint8_t *p = gc_ram_ptr(ram, 0x80300000u, 0x80);
    if (!p) die("gc_ram_ptr failed");
    for (u32 i = 0; i < 0x80; i += 4) wr32be(p + i, 0);

    u32 off = 0;

    /* --- Test 1: AIRegisterDMACallback --- */
    AIDCallback old1 = AIRegisterDMACallback(dummy_dma_cb);
    AIDCallback old2 = AIRegisterDMACallback((AIDCallback)0);
    wr32be(p + off, 0xA1CB0001u); off += 4;
    wr32be(p + off, (u32)(old1 == 0 ? 0 : 1)); off += 4;
    wr32be(p + off, (u32)(old2 != 0 ? 1 : 0)); off += 4;

    /* --- Test 2: AISetStreamVolLeft/Right + getters --- */
    AISetStreamVolLeft(0xAB);
    AISetStreamVolRight(0xCD);
    u8 vl = AIGetStreamVolLeft();
    u8 vr = AIGetStreamVolRight();
    wr32be(p + off, 0xA10A0002u); off += 4;
    wr32be(p + off, (u32)vl); off += 4;
    wr32be(p + off, (u32)vr); off += 4;
    wr32be(p + off, gc_ai_regs[1]); off += 4;

    /* --- Test 3: AIGetStreamSampleRate --- */
    u32 sr = AIGetStreamSampleRate();
    wr32be(p + off, 0xA15A0003u); off += 4;
    wr32be(p + off, sr); off += 4;

    /* --- Test 4: AISetStreamPlayState --- */
    AISetStreamPlayState(1);
    u32 ps1 = AIGetStreamPlayState();
    wr32be(p + off, 0xA1B50004u); off += 4;
    wr32be(p + off, ps1); off += 4;
    wr32be(p + off, gc_ai_regs[0]); off += 4;
    wr32be(p + off, gc_ai_regs[1]); off += 4;

    AISetStreamPlayState(0);
    u32 ps2 = AIGetStreamPlayState();
    wr32be(p + off, ps2); off += 4;
    wr32be(p + off, gc_ai_regs[0]); off += 4;

    AISetStreamPlayState(0);
    wr32be(p + off, gc_ai_regs[0]); off += 4;

    /* --- Test 5: AIInitDMA + AIStartDMA + AIGetDMAStartAddr --- */
    AIInitDMA(0x01234000u, 0x00008000u);
    AIStartDMA();
    u32 dma_addr = AIGetDMAStartAddr();
    wr32be(p + off, 0xA1DA0005u); off += 4;
    wr16be_local(p + off, gc_ai_dsp_regs[0]); off += 2;
    wr16be_local(p + off, gc_ai_dsp_regs[1]); off += 2;
    wr16be_local(p + off, gc_ai_dsp_regs[3]); off += 2;
    p[off] = 0; p[off+1] = 0; off += 2;
    wr32be(p + off, dma_addr); off += 4;

    /* --- Dump full observable state --- */
    wr32be(p + off, 0xA1FF0006u); off += 4;
    for (int i = 0; i < 4; i++) { wr32be(p + off, gc_ai_regs[i]); off += 4; }
    for (int i = 0; i < 4; i++) { wr16be_local(p + off, gc_ai_dsp_regs[i]); off += 2; }
}
