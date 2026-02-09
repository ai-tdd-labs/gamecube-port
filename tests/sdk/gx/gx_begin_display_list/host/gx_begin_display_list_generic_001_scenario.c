#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
void GXBeginDisplayList(void *list, u32 size);
u32 GXEndDisplayList(void);

extern u32 gc_gx_in_disp_list;
extern u32 gc_gx_dl_base;
extern u32 gc_gx_dl_size;
extern u32 gc_gx_dl_count;

const char *gc_scenario_label(void){return "GXBeginDisplayList/generic";}
const char *gc_scenario_out_path(void){return "../actual/gx_begin_display_list_generic_001.bin";}

void gc_scenario_run(GcRam *ram){
  // Use a stable GC address (aligned) so host and PPC match.
  (void)ram;
  GXBeginDisplayList((void *)0x80010000u,0x40);
  u32 count=GXEndDisplayList();
  uint8_t *p=gc_ram_ptr(ram,0x80300000u,0x40);
  if(!p) die("gc_ram_ptr failed");
  for(u32 i=0;i<0x40;i+=4) wr32be(p+i,0);
  wr32be(p+0x00,0xDEADBEEFu);
  wr32be(p+0x04,gc_gx_in_disp_list);
  wr32be(p+0x08,gc_gx_dl_base);
  wr32be(p+0x0C,gc_gx_dl_size);
  wr32be(p+0x10,gc_gx_dl_count);
  wr32be(p+0x14,count);
}
