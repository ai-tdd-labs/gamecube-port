#pragma once

#include <stdint.h>

// Host-only memory card backend used by sdk_port to simulate EXI/CARD storage.
// Not part of the Nintendo SDK API surface.

// Insert or create a raw memcard image for a channel.
// Returns 0 on success, <0 on failure.
int gc_memcard_insert(int chan, const char* path, uint32_t size_bytes);

// Eject a channel (frees in-memory image).
void gc_memcard_eject(int chan);

int gc_memcard_is_inserted(int chan);
uint32_t gc_memcard_size(int chan);

int gc_memcard_read(int chan, uint32_t off, void* dst, uint32_t len);
int gc_memcard_write(int chan, uint32_t off, const void* src, uint32_t len);

// Flush dirty image to disk (best-effort). Returns 0 on success.
int gc_memcard_flush(int chan);

