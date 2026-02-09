#include "game/jmp.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// process.c provides this; called when a process entry function returns.
void HuPrcEnd(void);

// Keep only the minimal metadata we need for host process bootstrap.
typedef struct HostJmpSlot {
    jmp_buf *key;
    void (*entry)(void);
    void *stack;
    size_t stack_size;
} HostJmpSlot;

enum { HOST_JMP_MAX = 256 };
static HostJmpSlot g_slots[HOST_JMP_MAX];
static u32 g_slots_n;

static HostJmpSlot *slot_for(jmp_buf *jump) {
    for (u32 i = 0; i < g_slots_n; i++) {
        if (g_slots[i].key == jump) return &g_slots[i];
    }
    if (g_slots_n >= HOST_JMP_MAX) {
        fprintf(stderr, "fatal: HostJmpSlot overflow\n");
        abort();
    }
    HostJmpSlot *s = &g_slots[g_slots_n++];
    memset(s, 0, sizeof(*s));
    s->key = jump;
    return s;
}

// Called by the assembly trampoline (host_process_trampoline) with the slot in x0.
void host_process_trampoline_c(HostJmpSlot *s) {
    if (!s || !s->entry) {
        fprintf(stderr, "fatal: process trampoline missing entry\n");
        abort();
    }
    s->entry();
    HuPrcEnd();
    abort();
}

// Host helper used by process.c to bootstrap a process context onto a fresh stack.
void gc_host_jmp_init_process(jmp_buf *jump, void (*entry)(void), u32 stack_size) {
    HostJmpSlot *s = slot_for(jump);
    s->entry = entry;

    // Keep stacks comfortably large; MP4 uses small defaults that are fine on
    // PPC but risky on host due to different call frames/tooling.
    if (stack_size < 65536u) stack_size = 65536u;
    s->stack_size = (size_t)stack_size;
    if (posix_memalign(&s->stack, 16, s->stack_size) != 0) s->stack = 0;
    if (!s->stack) {
        fprintf(stderr, "fatal: malloc stack failed (%d)\n", errno);
        abort();
    }
    memset(s->stack, 0xA5, s->stack_size);

    extern void host_process_trampoline(void); // defined in jmp_aarch64.S

    // Seed the initial saved context directly into the jmp_buf storage.
    memset(jump, 0, sizeof(*jump));
    jump->x19 = (uint64_t)(uintptr_t)s;
    jump->x30 = (uint64_t)(uintptr_t)host_process_trampoline;
    jump->sp = (uint64_t)(uintptr_t)((unsigned char *)s->stack + s->stack_size);
    jump->sp &= ~0xFULL;
    __asm__ volatile("mov %0, x18" : "=r"(jump->x18));

    if (getenv("GC_JMP_DEBUG")) {
        fprintf(stderr,
                "[gc_host_jmp_init_process] slot=%p jump=%p entry=%p sp=%#llx lr=%#llx\n",
                (void *)s, (void *)jump, (void *)entry,
                (unsigned long long)jump->sp, (unsigned long long)jump->x30);
    }
}
