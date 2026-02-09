#pragma once

#include "dolphin.h"

// MP4 uses a custom PPC-style jmp_buf. For host workloads, we repurpose the
// same storage size (248 bytes) to hold an AArch64 setjmp/longjmp environment
// so we can run cooperative HuPrc* scheduling deterministically.
//
// Critical: gcsetjmp must save the *callsite* stack pointer. If we save the SP
// of a wrapper function (with its own prologue), optimized builds corrupt the
// stack on resume. Therefore, host gcsetjmp/gclongjmp are implemented in
// assembly as leaf functions.
#ifdef GC_HOST_WORKLOAD
typedef struct __attribute__((aligned(16))) jump_buf {
    // 0x00..0x6F: callee-saved GPRs + SP/TLS platform reg
    uint64_t sp;   // 0x00
    uint64_t x18;  // 0x08 (Apple platform register; must be preserved)
    uint64_t x19;  // 0x10
    uint64_t x20;  // 0x18
    uint64_t x21;  // 0x20
    uint64_t x22;  // 0x28
    uint64_t x23;  // 0x30
    uint64_t x24;  // 0x38
    uint64_t x25;  // 0x40
    uint64_t x26;  // 0x48
    uint64_t x27;  // 0x50
    uint64_t x28;  // 0x58
    uint64_t x29;  // 0x60 (FP)
    uint64_t x30;  // 0x68 (LR)

    // 0x70..0xEF: callee-saved SIMD/FP regs (AArch64 ABI: v8-v15).
    uint8_t v8[16];   // 0x70
    uint8_t v9[16];   // 0x80
    uint8_t v10[16];  // 0x90
    uint8_t v11[16];  // 0xA0
    uint8_t v12[16];  // 0xB0
    uint8_t v13[16];  // 0xC0
    uint8_t v14[16];  // 0xD0
    uint8_t v15[16];  // 0xE0

    // Pad to 248 bytes to match the original MP4 storage size.
    uint64_t _pad;  // 0xF0
} jmp_buf;
#else
typedef struct jump_buf {
    u32 lr;
    u32 cr;
    u32 sp;
    u32 r2;
    u32 pad;
    u32 regs[19];
    double flt_regs[19];
} jmp_buf;
#endif

// These behave like setjmp/longjmp. On host, we must tell the compiler about
// the non-local control flow; otherwise optimized code may assume "returns once"
// and keep live values in registers across yields, which becomes UB on resume.
#ifdef GC_HOST_WORKLOAD
__attribute__((returns_twice)) s32 gcsetjmp(jmp_buf *jump);
__attribute__((noreturn)) s32 gclongjmp(jmp_buf *jump, s32 status);
#else
s32 gcsetjmp(jmp_buf *jump);
s32 gclongjmp(jmp_buf *jump, s32 status);
#endif
