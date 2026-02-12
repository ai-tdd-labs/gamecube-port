/*
 * sdk_port/ai/ai.h --- Host-side port of GC SDK AI (audio interface).
 *
 * Source of truth: external/mp4-decomp/src/dolphin/ai.c
 *
 * Models __AIRegs[0..3] and __DSPRegs[24..27] as observable state.
 * Hardware timing (__AI_SRC_INIT) is a no-op.
 */
#pragma once

#include <stdint.h>

typedef void (*AIDCallback)(void);

/* Observable AI register state. */
extern uint32_t gc_ai_regs[4];      /* __AIRegs[0..3] */
extern uint16_t gc_ai_dsp_regs[4];  /* __DSPRegs[24..27] */
extern uintptr_t gc_ai_dma_cb_ptr;

void AIInitDMA(uint32_t addr, uint32_t length);
void AIStartDMA(void);
uint32_t AIGetDMAStartAddr(void);
AIDCallback AIRegisterDMACallback(AIDCallback callback);
void AISetStreamPlayState(uint32_t state);
uint32_t AIGetStreamPlayState(void);
void AISetStreamVolLeft(uint8_t volume);
uint8_t AIGetStreamVolLeft(void);
void AISetStreamVolRight(uint8_t volume);
uint8_t AIGetStreamVolRight(void);
uint32_t AIGetStreamSampleRate(void);
