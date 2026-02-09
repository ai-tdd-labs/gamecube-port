#include "game/jmp.h"

// Host-only implementation of the MP4 "gcsetjmp/gclongjmp" ABI.
//
// The MP4 decomp uses a PPC-style jmp_buf struct to save/restore execution
// context and to bootstrap "process" coroutines onto a fresh stack.
//
// On host we can't rely on PPC register layouts, so we implement equivalent
// behavior using ucontext:
// - scheduler context: saved/restored via getcontext/setcontext
// - process contexts: created via makecontext on a malloc'd host stack
//
// This is intentionally minimal: enough to run HuPrcCall deterministically.

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ucontext.h>

// process.c provides these; called when a process entry function returns.
void HuPrcEnd(void);

typedef struct HostJmpSlot {
    jmp_buf *key;
    u32 is_process;
    u32 started;
    u32 initialized;
    u32 ret_pending;
    s32 ret_value;

    ucontext_t ctx;

    // For process slots:
    void (*entry)(void);
    void *stack;
    size_t stack_size;
} HostJmpSlot;

enum { HOST_JMP_MAX = 128 };
static HostJmpSlot g_slots[HOST_JMP_MAX];
static u32 g_slots_n;
static const uint64_t g_slots_canary = 0x4A4D505F484F5354ull; // "JMP_HOST"

static HostJmpSlot *slot_for(jmp_buf *jump) {
    if (g_slots_canary != 0x4A4D505F484F5354ull) {
        fprintf(stderr, "fatal: HostJmpSlot canary corrupted\n");
        abort();
    }
    for (u32 i = 0; i < g_slots_n; i++) {
        if (g_slots[i].key == jump) {
            if (getenv("GC_JMP_DEBUG")) {
                fprintf(stderr,
                        "[slot_for] hit i=%u key=%p is_process=%u started=%u init=%u\n",
                        i, (void *)jump, g_slots[i].is_process, g_slots[i].started,
                        g_slots[i].initialized);
            }
            return &g_slots[i];
        }
    }
    if (g_slots_n >= HOST_JMP_MAX) {
        fprintf(stderr, "fatal: HostJmpSlot overflow\n");
        abort();
    }
    HostJmpSlot *s = &g_slots[g_slots_n++];
    memset(s, 0, sizeof(*s));
    s->key = jump;
    if (getenv("GC_JMP_DEBUG")) {
        fprintf(stderr, "[slot_for] new i=%u key=%p\n", g_slots_n - 1, (void *)jump);
    }
    return s;
}

static void process_trampoline(uintptr_t slot_index) {
    if (slot_index >= g_slots_n) {
        fprintf(stderr, "fatal: bad process slot index\n");
        abort();
    }
    HostJmpSlot *s = &g_slots[slot_index];
    s->started = 1;
    if (!s->entry) {
        fprintf(stderr, "fatal: process slot missing entry\n");
        abort();
    }
    s->entry();

    // If a process function ever returns, terminate the process like the PPC
    // runtime would by jumping into HuPrcEnd().
    HuPrcEnd();
    abort();
}

// Host helper used by process.c to bootstrap a process slot onto a fresh stack.
void gc_host_jmp_init_process(jmp_buf *jump, void (*entry)(void), u32 stack_size) {
    HostJmpSlot *s = slot_for(jump);
    s->is_process = 1;
    s->entry = entry;

    // ucontext on modern macOS is deprecated and fragile with small stacks.
    // Use a larger minimum stack to avoid silent stack corruption.
    if (stack_size < 65536u) stack_size = 65536u;
    s->stack_size = (size_t)stack_size;
    // Ensure 16-byte alignment for ABI + makecontext.
    if (posix_memalign(&s->stack, 16, s->stack_size) != 0) s->stack = 0;
    if (!s->stack) {
        fprintf(stderr, "fatal: malloc stack failed\n");
        abort();
    }
    // Fill with a recognizable pattern for debugging. (Does not affect behavior.)
    memset(s->stack, 0xA5, s->stack_size);

    if (getcontext(&s->ctx) != 0) {
        fprintf(stderr, "fatal: getcontext failed: %s\n", strerror(errno));
        abort();
    }
    s->ctx.uc_stack.ss_sp = s->stack;
    s->ctx.uc_stack.ss_size = s->stack_size;
    s->ctx.uc_stack.ss_flags = 0;
    s->ctx.uc_link = 0;
    makecontext(&s->ctx, (void (*)(void))process_trampoline, 1, (uintptr_t)(s - g_slots));
    s->initialized = 1;
}

s32 gcsetjmp(jmp_buf *jump) {
    HostJmpSlot *s = slot_for(jump);

    // setjmp returns twice: first 0, later the value supplied by longjmp.
    if (getcontext(&s->ctx) != 0) {
        fprintf(stderr, "fatal: getcontext failed: %s\n", strerror(errno));
        abort();
    }
    s->initialized = 1;

    if (s->ret_pending) {
        s->ret_pending = 0;
        if (getenv("GC_JMP_DEBUG")) {
            fprintf(stderr, "[gcsetjmp] return-twice key=%p value=%d\n",
                    (void *)jump, s->ret_value);
        }
        return s->ret_value;
    }
    if (getenv("GC_JMP_DEBUG")) {
        fprintf(stderr, "[gcsetjmp] first-return key=%p\n", (void *)jump);
    }
    return 0;
}

s32 gclongjmp(jmp_buf *jump, s32 status) {
    HostJmpSlot *s = slot_for(jump);
    if (getenv("GC_JMP_DEBUG")) {
        fprintf(stderr,
                "[gclongjmp] key=%p status=%d is_process=%u started=%u init=%u\n",
                (void *)jump, status, s->is_process, s->started, s->initialized);
    }
    if (!s->initialized) {
        fprintf(stderr, "fatal: longjmp to uninitialized slot\n");
        abort();
    }
    // Starting a never-run process context is not a "return-from-setjmp" event;
    // it should begin at the entry trampoline. Only set ret_pending for contexts
    // that were captured by gcsetjmp (scheduler, or a yielded process).
    if (!s->is_process || s->started) {
        s->ret_value = status;
        s->ret_pending = 1;
    }

    // setcontext does not return on success.
    if (setcontext(&s->ctx) != 0) {
        fprintf(stderr, "fatal: setcontext failed: %s\n", strerror(errno));
        abort();
    }
    abort();
}
