#include <stdint.h>

typedef void (*ARCallback)(void);

uintptr_t gc_ar_callback_ptr;

void oracle_ARRegisterDMACallback_reset(uint32_t s) {
    gc_ar_callback_ptr = (uintptr_t)(0x80000000u | (s & 0x00FFFFFFu));
}

ARCallback oracle_ARRegisterDMACallback(ARCallback callback) {
    ARCallback old = (ARCallback)gc_ar_callback_ptr;
    gc_ar_callback_ptr = (uintptr_t)callback;
    return old;
}
