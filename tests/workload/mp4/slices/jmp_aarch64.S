// aarch64 context save/restore used by tests/workload/mp4/slices/jmp_host.c
//
// The storage blob is the existing MP4 jmp_buf struct (248 bytes). We overlay
// a host context layout onto it. Offsets must match jmp_host.c.
//
// Layout (byte offsets): 31 * 8 bytes = 248 bytes total
//   slot  0: x0   (return value on restore)
//   slot  1: x1
//   ...
//   slot 17: sp   (instead of x17)
//   slot 18: x18  (Apple platform register; must be preserved)
//   slot 19: x19
//   ...
//   slot 30: x30  (LR / return address used by `ret`)

.text
.align 2

// Minimal aarch64 context save/restore used by tests/workload/mp4/slices/jmp_host.c.
//
// We store context in HostJmpSlot.ctx (NOT inside MP4's jmp_buf), because MP4
// code may legitimately write into jmp_buf PPC fields and would corrupt us.
//
// Context layout (HostJmpSlot.ctx):
//   0x00: sp
//   0x08: x18
//   0x10: x19
//   0x18: x20
//   ...
//   0x60: x29
//   0x68: x30
//   0x70: v8
//   0x80: v9
//   0x90: v10
//   0xA0: v11
//   0xB0: v12
//   0xC0: v13
//   0xD0: v14
//   0xE0: v15

.global _gc_ctx_save_slot
_gc_ctx_save_slot:
    // x0 = ctx*
    mov x9, sp
    str x9,  [x0, #0x00]           // sp
    str x18, [x0, #0x08]           // x18
    stp x19, x20, [x0, #0x10]
    stp x21, x22, [x0, #0x20]
    stp x23, x24, [x0, #0x30]
    stp x25, x26, [x0, #0x40]
    stp x27, x28, [x0, #0x50]
    stp x29, x30, [x0, #0x60]
    stp q8,  q9,  [x0, #0x70]
    stp q10, q11, [x0, #0x90]
    stp q12, q13, [x0, #0xB0]
    stp q14, q15, [x0, #0xD0]
    mov w0, wzr
    ret

.global _gc_ctx_restore_slot
_gc_ctx_restore_slot:
    // x0 = ctx*
    // w1 = status
    cbnz w1, 1f
    mov w1, #1
1:
    ldr x9,  [x0, #0x00]
    mov sp, x9
    ldr x18, [x0, #0x08]
    ldp x19, x20, [x0, #0x10]
    ldp x21, x22, [x0, #0x20]
    ldp x23, x24, [x0, #0x30]
    ldp x25, x26, [x0, #0x40]
    ldp x27, x28, [x0, #0x50]
    ldp x29, x30, [x0, #0x60]
    ldp q8,  q9,  [x0, #0x70]
    ldp q10, q11, [x0, #0x90]
    ldp q12, q13, [x0, #0xB0]
    ldp q14, q15, [x0, #0xD0]
    mov w0, w1
    ret

// Host implementation of MP4 gcsetjmp/gclongjmp.
//
// These must be leaf functions (no prologue/epilogue) so the saved SP is the
// caller's SP at the callsite. If we saved the SP of a C wrapper (after its
// prologue), optimized builds would restore the wrong stack pointer and corrupt
// control flow.
//
// Layout is the host jmp_buf in tests/workload/include/game/jmp.h:
//   0x00: sp
//   0x08: x18
//   0x10: x19..x28 (pairs)
//   0x60: x29
//   0x68: x30
//   0x70: v8..v15 (pairs q8/q9, q10/q11, q12/q13, q14/q15)
.global _gcsetjmp
_gcsetjmp:
    // x0 = jmp_buf*
    mov x9, sp
    str x9,  [x0, #0x00]           // sp
    str x18, [x0, #0x08]           // x18
    stp x19, x20, [x0, #0x10]
    stp x21, x22, [x0, #0x20]
    stp x23, x24, [x0, #0x30]
    stp x25, x26, [x0, #0x40]
    stp x27, x28, [x0, #0x50]
    stp x29, x30, [x0, #0x60]
    stp q8,  q9,  [x0, #0x70]
    stp q10, q11, [x0, #0x90]
    stp q12, q13, [x0, #0xB0]
    stp q14, q15, [x0, #0xD0]
    mov w0, wzr
    ret

.global _gclongjmp
_gclongjmp:
    // x0 = jmp_buf*
    // w1 = status
    cbnz w1, 1f
    mov w1, #1
1:
    ldr x9,  [x0, #0x00]
    mov sp, x9
    ldr x18, [x0, #0x08]
    ldp x19, x20, [x0, #0x10]
    ldp x21, x22, [x0, #0x20]
    ldp x23, x24, [x0, #0x30]
    ldp x25, x26, [x0, #0x40]
    ldp x27, x28, [x0, #0x50]
    ldp x29, x30, [x0, #0x60]
    ldp q8,  q9,  [x0, #0x70]
    ldp q10, q11, [x0, #0x90]
    ldp q12, q13, [x0, #0xB0]
    ldp q14, q15, [x0, #0xD0]
    mov w0, w1
    ret

.global _gc_ctx_pac_sign_lr
_gc_ctx_pac_sign_lr:
    // x0 = ctx*
    // No-op on host builds: our host test binaries are not compiled with
    // pointer authentication / branch protection, so signing/authenticating
    // LR would corrupt the return address.
    ret

.global _gc_ctx_save
_gc_ctx_save:
    // x0 = ctx_blob
    // Save a deterministic "initial return" value for setjmp.
    str xzr, [x0, #0x00]           // x0
    stp x1,  x2,  [x0, #0x08]      // x1-x2
    stp x3,  x4,  [x0, #0x18]      // x3-x4
    stp x5,  x6,  [x0, #0x28]      // x5-x6
    stp x7,  x8,  [x0, #0x38]      // x7-x8
    str x9,  [x0, #0x48]           // x9
    str x10, [x0, #0x50]           // x10
    stp x11, x12, [x0, #0x58]      // x11-x12
    stp x13, x14, [x0, #0x68]      // x13-x14
    stp x15, x16, [x0, #0x78]      // x15-x16
    mov x9, sp
    str x9,  [x0, #0x88]           // sp (slot 17)
    str x18, [x0, #0x90]           // x18 (slot 18)
    stp x19, x20, [x0, #0x98]      // x19-x20
    stp x21, x22, [x0, #0xA8]      // x21-x22
    stp x23, x24, [x0, #0xB8]      // x23-x24
    stp x25, x26, [x0, #0xC8]      // x25-x26
    stp x27, x28, [x0, #0xD8]      // x27-x28
    stp x29, x30, [x0, #0xE8]      // x29-x30
    mov w0, wzr
    ret

.global _gc_ctx_restore
_gc_ctx_restore:
    // x0 = ctx_blob
    ldp x1,  x2,  [x0, #0x08]
    ldp x3,  x4,  [x0, #0x18]
    ldp x5,  x6,  [x0, #0x28]
    ldp x7,  x8,  [x0, #0x38]
    ldr x9,  [x0, #0x48]
    ldr x10, [x0, #0x50]
    ldp x11, x12, [x0, #0x58]
    ldp x13, x14, [x0, #0x68]
    ldp x15, x16, [x0, #0x78]
    ldr x17, [x0, #0x88]           // sp
    mov sp, x17
    ldr x18, [x0, #0x90]           // x18
    ldp x19, x20, [x0, #0x98]
    ldp x21, x22, [x0, #0xA8]
    ldp x23, x24, [x0, #0xB8]
    ldp x25, x26, [x0, #0xC8]
    ldp x27, x28, [x0, #0xD8]
    ldp x29, x30, [x0, #0xE8]
    ldr x0, [x0, #0x00]
    ret

// Process bootstrap trampoline.
// We seed x19 with HostJmpSlot* during gc_host_jmp_init_process().
// Entering through this symbol avoids any compiler prologue clobbering x19.
.global _host_process_trampoline
_host_process_trampoline:
    mov x0, x19
    bl _host_process_trampoline_c
    // Should never return.
    brk #0x1
