#include "memcard_backend.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Keep this host-only backend decoupled from sdk headers; define the EXI type
// values we need locally.
enum { GC_EXI_READ = 0, GC_EXI_WRITE = 1 };

typedef struct GcMemcard {
  int inserted;
  uint32_t size;
  uint8_t* data;
  int dirty;
  char path[512];
} GcMemcard;

static GcMemcard s_cards[2];

static void clear_card(GcMemcard* c) {
  if (c->data) {
    free(c->data);
  }
  memset(c, 0, sizeof(*c));
}

static int load_or_create(const char* path, uint8_t* data, uint32_t size) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    // Create a new image as 0xFF-filled (unformatted). CARDFormat will define real contents.
    memset(data, 0xFF, size);
    f = fopen(path, "wb");
    if (!f) return -1;
    size_t wr = fwrite(data, 1, size, f);
    fclose(f);
    return (wr == size) ? 0 : -1;
  }

  size_t rd = fread(data, 1, size, f);
  fclose(f);
  if (rd != size) {
    // If file is shorter, pad with 0xFF; if longer, ignore tail.
    if (rd < size) memset(data + rd, 0xFF, size - (uint32_t)rd);
  }
  return 0;
}

int gc_memcard_insert(int chan, const char* path, uint32_t size_bytes) {
  if (chan < 0 || chan >= 2) return -1;
  if (!path || !*path) return -1;
  if (size_bytes == 0) return -1;
  if (size_bytes > (32u * 1024u * 1024u)) return -1;

  GcMemcard* c = &s_cards[chan];
  clear_card(c);

  c->data = (uint8_t*)malloc(size_bytes);
  if (!c->data) return -1;
  c->size = size_bytes;

  // Store path.
  size_t n = strlen(path);
  if (n >= sizeof(c->path)) n = sizeof(c->path) - 1;
  memcpy(c->path, path, n);
  c->path[n] = 0;

  if (load_or_create(c->path, c->data, c->size) != 0) {
    clear_card(c);
    return -1;
  }

  c->inserted = 1;
  c->dirty = 0;
  return 0;
}

void gc_memcard_eject(int chan) {
  if (chan < 0 || chan >= 2) return;
  clear_card(&s_cards[chan]);
}

int gc_memcard_is_inserted(int chan) {
  if (chan < 0 || chan >= 2) return 0;
  return s_cards[chan].inserted ? 1 : 0;
}

uint32_t gc_memcard_size(int chan) {
  if (chan < 0 || chan >= 2) return 0;
  return s_cards[chan].size;
}

int gc_memcard_read(int chan, uint32_t off, void* dst, uint32_t len) {
  if (chan < 0 || chan >= 2) return -1;
  if (!dst && len) return -1;
  GcMemcard* c = &s_cards[chan];
  if (!c->inserted || !c->data) return -1;
  if (off > c->size || len > c->size - off) return -1;
  if (len) memcpy(dst, c->data + off, len);
  return 0;
}

int gc_memcard_write(int chan, uint32_t off, const void* src, uint32_t len) {
  if (chan < 0 || chan >= 2) return -1;
  if (!src && len) return -1;
  GcMemcard* c = &s_cards[chan];
  if (!c->inserted || !c->data) return -1;
  if (off > c->size || len > c->size - off) return -1;
  if (len) memcpy(c->data + off, src, len);
  c->dirty = 1;
  return 0;
}

int gc_memcard_flush(int chan) {
  if (chan < 0 || chan >= 2) return -1;
  GcMemcard* c = &s_cards[chan];
  if (!c->inserted || !c->data) return -1;
  if (!c->dirty) return 0;

  FILE* f = fopen(c->path, "wb");
  if (!f) return -1;
  size_t wr = fwrite(c->data, 1, c->size, f);
  fclose(f);
  if (wr != c->size) return -1;
  c->dirty = 0;
  return 0;
}

int gc_memcard_exi_dma(int chan, uint32_t exi_addr, void* buffer, int length, uint32_t type) {
  if (!buffer || length <= 0) return 0;
  if (!gc_memcard_is_inserted(chan)) return 0;

  // EXI/CARD address is already a byte offset in the CARDBios command encoding.
  uint32_t len = (uint32_t)length;
  if (type == (uint32_t)GC_EXI_READ) {
    return gc_memcard_read(chan, exi_addr, buffer, len) == 0;
  }
  if (type == (uint32_t)GC_EXI_WRITE) {
    return gc_memcard_write(chan, exi_addr, buffer, len) == 0;
  }
  return 0;
}
