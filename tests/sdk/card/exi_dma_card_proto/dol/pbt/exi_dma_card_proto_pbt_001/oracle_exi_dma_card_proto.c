#include <stdint.h>
#include <string.h>

typedef uint32_t u32;
typedef int32_t s32;

enum { EXI_READ = 0, EXI_WRITE = 1 };

static int s_selected;
static int s_has_addr;
static u32 s_addr;

// Tiny "device image" used as the oracle backing store.
static uint8_t s_card_img[0x4000];

void oracle_EXIInit(void) {
  s_selected = 0;
  s_has_addr = 0;
  s_addr = 0;
  for (u32 i = 0; i < (u32)sizeof(s_card_img); i++) s_card_img[i] = (uint8_t)(i ^ 0xA5u);
  // Seed a known read window.
  for (u32 i = 0; i < 512u; i++) s_card_img[0x1000u + i] = (uint8_t)(0xC0u ^ (uint8_t)i);
}

int oracle_EXILock(s32 chan, u32 dev, void* cb) { (void)chan; (void)dev; (void)cb; return 1; }
int oracle_EXIUnlock(s32 chan) { (void)chan; return 1; }
int oracle_EXISelect(s32 chan, u32 dev, u32 freq) { (void)chan; (void)dev; (void)freq; s_selected = 1; return 1; }
int oracle_EXIDeselect(s32 chan) { (void)chan; s_selected = 0; return 1; }

static u32 decode_addr(const uint8_t* b) {
  return (((u32)(b[1] & 0x7Fu)) << 17) |
         (((u32)b[2]) << 9) |
         (((u32)(b[3] & 0x03u)) << 7) |
         ((u32)(b[4] & 0x7Fu));
}

int oracle_EXIImmEx(s32 chan, void* buf, s32 len, u32 type) {
  (void)chan;
  if (type != EXI_READ && buf && len >= 5) {
    const uint8_t* b = (const uint8_t*)buf;
    if (b[0] == 0x52 || b[0] == 0xF2 || b[0] == 0xF1) {
      s_addr = decode_addr(b);
      s_has_addr = 1;
    }
  }
  return 1;
}

int oracle_EXIDma(s32 chan, void* buffer, s32 length, u32 type, void* cb) {
  (void)chan;
  (void)cb;
  if (!s_selected || !s_has_addr) return 0;
  if (!buffer || length <= 0) return 0;
  u32 len = (u32)length;
  if (s_addr + len > (u32)sizeof(s_card_img)) return 0;

  if (type == EXI_READ) {
    memcpy(buffer, &s_card_img[s_addr], len);
    return 1;
  }
  if (type == EXI_WRITE) {
    memcpy(&s_card_img[s_addr], buffer, len);
    return 1;
  }
  return 0;
}

// Helper for tests: read back from oracle backing store.
int oracle_card_peek(u32 addr, void* dst, u32 len) {
  if (!dst || addr + len > (u32)sizeof(s_card_img)) return 0;
  memcpy(dst, &s_card_img[addr], len);
  return 1;
}

