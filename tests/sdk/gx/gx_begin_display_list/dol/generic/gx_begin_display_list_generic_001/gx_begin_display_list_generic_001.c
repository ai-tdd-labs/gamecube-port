#include <stdint.h>
typedef uint32_t u32;
void GXBeginDisplayList(void *list, u32 size);
u32 GXEndDisplayList(void);

extern u32 gc_gx_in_disp_list;
extern u32 gc_gx_dl_base;
extern u32 gc_gx_dl_size;
extern u32 gc_gx_dl_count;

static inline void wr32be(volatile uint8_t *p, u32 v) {
  p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)(v);
}

int main(void){
  volatile uint8_t *out=(volatile uint8_t*)0x80300000;
  for(u32 i=0;i<0x40;i++) out[i]=0;

  // Use a stable GC address (aligned) so host and PPC match.
  GXBeginDisplayList((void *)0x80010000u, 0x40);
  u32 count = GXEndDisplayList();

  wr32be(out+0x00,0xDEADBEEFu);
  wr32be(out+0x04,gc_gx_in_disp_list);
  wr32be(out+0x08,gc_gx_dl_base);
  wr32be(out+0x0C,gc_gx_dl_size);
  wr32be(out+0x10,gc_gx_dl_count);
  wr32be(out+0x14,count);
  while(1){}
}
