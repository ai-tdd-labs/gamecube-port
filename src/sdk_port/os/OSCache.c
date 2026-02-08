#include <stdint.h>

// Minimal cache API surface for deterministic tests.
// We do not model cache behavior; we only record the call args.

uint32_t gc_dc_store_last_addr;
uint32_t gc_dc_store_last_len;

uint32_t gc_dc_inval_last_addr;
uint32_t gc_dc_inval_last_len;

uint32_t gc_dc_flush_last_addr;
uint32_t gc_dc_flush_last_len;

// NOTE:
// - Host builds: deterministic "record only" behavior (no cache modeling).
// - PPC DOL builds: libogc also defines DCStoreRangeNoSync. Mark this weak so
//   the linker can pick the platform implementation without duplicate symbols.
__attribute__((weak)) void DCStoreRangeNoSync(void *addr, uint32_t nbytes) {
    gc_dc_store_last_addr = (uint32_t)(uintptr_t)addr;
    gc_dc_store_last_len = nbytes;
}

// Used by MP4 callsites (dvd.c, thp, gfx code). We record args for deterministic tests.
__attribute__((weak)) void DCInvalidateRange(void *addr, uint32_t nbytes) {
    gc_dc_inval_last_addr = (uint32_t)(uintptr_t)addr;
    gc_dc_inval_last_len = nbytes;
}

__attribute__((weak)) void DCFlushRange(void *addr, uint32_t nbytes) {
    gc_dc_flush_last_addr = (uint32_t)(uintptr_t)addr;
    gc_dc_flush_last_len = nbytes;
}
