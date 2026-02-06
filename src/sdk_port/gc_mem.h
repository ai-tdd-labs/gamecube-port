#pragma once

#include <stddef.h>
#include <stdint.h>

// Minimal "virtual RAM" mapper for sdk_port.
// Host runner sets the backing buffer before running scenarios.

void gc_mem_set(uint32_t base, size_t size, uint8_t *buf);
uint8_t *gc_mem_ptr(uint32_t addr, size_t len);

