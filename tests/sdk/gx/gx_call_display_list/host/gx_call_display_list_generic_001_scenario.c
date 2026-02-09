#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32;
void GXCallDisplayList(const void *list, u32 nbytes);
extern u32 gc_gx_call_dl_list;
extern u32 gc_gx_call_dl_nbytes;
const char *gc_scenario_label(void){return "GXCallDisplayList/generic";}
const char *gc_scenario_out_path(void){return "../actual/gx_call_display_list_generic_001.bin";}

void gc_scenario_run(GcRam *ram){
  (void)ram;
  GXCallDisplayList((void *)0x80010000u,0x40);
  uint8_t *p=gc_ram_ptr(ram,0x80300000u,0x40);
  if(!p) die("gc_ram_ptr failed");
  for(u32 i=0;i<0x40;i+=4) wr32be(p+i,0);
  wr32be(p+0x00,0xDEADBEEFu);
  wr32be(p+0x04,gc_gx_call_dl_list);
  wr32be(p+0x08,gc_gx_call_dl_nbytes);
}
