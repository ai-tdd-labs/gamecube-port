#include <stdint.h>

// Minimal cache API surface for deterministic tests.
// We do not model cache behavior; we only record the call args.

uint32_t gc_dc_store_last_addr;
uint32_t gc_dc_store_last_len;

void DCStoreRangeNoSync(void *addr, uint32_t nbytes) {
    gc_dc_store_last_addr = (uint32_t)(uintptr_t)addr;
    gc_dc_store_last_len = nbytes;
}

