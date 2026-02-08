#include "gc_host_ram.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static void mkdir_p_for_file(const char *path) {
    if (!path) return;
    // Create parent directories for a file path. Best-effort.
    char tmp[4096];
    size_t n = strlen(path);
    if (n >= sizeof(tmp)) return;
    memcpy(tmp, path, n + 1);

    for (size_t i = 1; i < n; i++) {
        if (tmp[i] == '/') {
            tmp[i] = 0;
            (void)mkdir(tmp, 0777);
            tmp[i] = '/';
        }
    }
}

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

    mkdir_p_for_file(out_path);
    FILE *f = fopen(out_path, "wb");
    if (!f) return -1;
    size_t n = fwrite(p, 1, len, f);
    fclose(f);
    return (n == len) ? 0 : -1;
}
