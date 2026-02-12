#include <stdint.h>

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

static inline void wr32be(volatile uint8_t *p, u32 v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)(v);
}
static inline void wr16be(volatile uint8_t *p, u16 v) {
    p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)(v);
}

static void dummy_dma_cb(void) {}

int main(void) {
    volatile uint8_t *out = (volatile uint8_t *)0x80300000;
    for (u32 i = 0; i < 0x80; i++) out[i] = 0;

    u32 off = 0;

    /* --- Test 1: AIRegisterDMACallback --- */
    AIDCallback old1 = AIRegisterDMACallback(dummy_dma_cb);
    AIDCallback old2 = AIRegisterDMACallback((AIDCallback)0);
    wr32be(out + off, 0xA1CB0001u); off += 4; /* tag */
    wr32be(out + off, (u32)(old1 == 0 ? 0 : 1)); off += 4;
    wr32be(out + off, (u32)(old2 != 0 ? 1 : 0)); off += 4;
    /* subtotal: 12 bytes */

    /* --- Test 2: AISetStreamVolLeft/Right + getters --- */
    AISetStreamVolLeft(0xAB);
    AISetStreamVolRight(0xCD);
    u8 vl = AIGetStreamVolLeft();
    u8 vr = AIGetStreamVolRight();
    wr32be(out + off, 0xA10A0002u); off += 4; /* tag */
    wr32be(out + off, (u32)vl); off += 4;
    wr32be(out + off, (u32)vr); off += 4;
    wr32be(out + off, gc_ai_regs[1]); off += 4;
    /* subtotal: 28 bytes */

    /* --- Test 3: AIGetStreamSampleRate (should be 0 initially) --- */
    u32 sr = AIGetStreamSampleRate();
    wr32be(out + off, 0xA15A0003u); off += 4; /* tag */
    wr32be(out + off, sr); off += 4;
    /* subtotal: 36 bytes */

    /* --- Test 4: AISetStreamPlayState --- */
    /* Play state 1 with sample rate 0 triggers __AI_SRC_INIT path. */
    AISetStreamPlayState(1);
    u32 ps1 = AIGetStreamPlayState();
    wr32be(out + off, 0xA1B50004u); off += 4; /* tag */
    wr32be(out + off, ps1); off += 4;
    wr32be(out + off, gc_ai_regs[0]); off += 4;
    wr32be(out + off, gc_ai_regs[1]); off += 4; /* volumes after SRC_INIT restore */

    /* Play state 0 (normal path). */
    AISetStreamPlayState(0);
    u32 ps2 = AIGetStreamPlayState();
    wr32be(out + off, ps2); off += 4;
    wr32be(out + off, gc_ai_regs[0]); off += 4;

    /* Set to 0 again — same state, should be no-op. */
    AISetStreamPlayState(0);
    wr32be(out + off, gc_ai_regs[0]); off += 4;
    /* subtotal: 64 bytes */

    /* --- Test 5: AIInitDMA + AIStartDMA + AIGetDMAStartAddr --- */
    AIInitDMA(0x01234000u, 0x00008000u);
    AIStartDMA();
    u32 dma_addr = AIGetDMAStartAddr();
    wr32be(out + off, 0xA1DA0005u); off += 4; /* tag */
    wr16be(out + off, gc_ai_dsp_regs[0]); off += 2;
    wr16be(out + off, gc_ai_dsp_regs[1]); off += 2;
    wr16be(out + off, gc_ai_dsp_regs[3]); off += 2;
    out[off] = 0; out[off+1] = 0; off += 2; /* pad to 4-byte align */
    wr32be(out + off, dma_addr); off += 4;
    /* subtotal: 80 bytes */

    /* --- Dump full observable state --- */
    wr32be(out + off, 0xA1FF0006u); off += 4; /* tag */
    for (int i = 0; i < 4; i++) { wr32be(out + off, gc_ai_regs[i]); off += 4; }
    for (int i = 0; i < 4; i++) { wr16be(out + off, gc_ai_dsp_regs[i]); off += 2; }
    /* subtotal: 108 = 0x6C bytes */

    while (1) { __asm__ volatile("nop"); }
    return 0;
}
