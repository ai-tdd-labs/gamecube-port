#include "gc_host_ram.h"

#include <string.h>

int gc_ram_init(GcRam *ram, uint32_t base, size_t size) {
    memset(ram, 0, sizeof(*ram));
    ram->base = base;
    ram->size = size;
    ram->buf = (uint8_t *)calloc(1, size);
    return ram->buf ? 0 : -1;
}

void gc_ram_free(GcRam *ram) {
    if (ram->buf) {
        free(ram->buf);
        ram->buf = 0;
    }
}

uint8_t *gc_ram_ptr(GcRam *ram, uint32_t addr, size_t len) {
    if (addr < ram->base) return 0;
    uint64_t off = (uint64_t)addr - (uint64_t)ram->base;
    if (off + len > ram->size) return 0;
    return &ram->buf[off];
}

int gc_ram_dump(GcRam *ram, uint32_t addr, size_t len, const char *out_path) {
    uint8_t *p = gc_ram_ptr(ram, addr, len);
    if (!p) return -1;

    FILE *f = fopen(out_path, "wb");
    if (!f) return -1;
    size_t n = fwrite(p, 1, len, f);
    fclose(f);
    return (n == len) ? 0 : -1;
}

