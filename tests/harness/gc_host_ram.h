#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct GcRam {
    uint32_t base;
    size_t size;
    uint8_t *buf;
} GcRam;

int gc_ram_init(GcRam *ram, uint32_t base, size_t size);
void gc_ram_free(GcRam *ram);
uint8_t *gc_ram_ptr(GcRam *ram, uint32_t addr, size_t len);
int gc_ram_dump(GcRam *ram, uint32_t addr, size_t len, const char *out_path);

