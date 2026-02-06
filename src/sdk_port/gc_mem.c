#include "gc_mem.h"

static uint32_t g_base;
static size_t g_size;
static uint8_t *g_buf;

void gc_mem_set(uint32_t base, size_t size, uint8_t *buf) {
    g_base = base;
    g_size = size;
    g_buf = buf;
}

uint8_t *gc_mem_ptr(uint32_t addr, size_t len) {
    if (!g_buf) return 0;
    if (addr < g_base) return 0;
    uint64_t off = (uint64_t)addr - (uint64_t)g_base;
    if (off + len > g_size) return 0;
    return &g_buf[off];
}

