#include <stdint.h>
#include "harness/gc_host_ram.h"
#include "harness/gc_host_scenario.h"
#include "harness/gc_host_test.h"

typedef uint32_t u32; typedef int16_t s16;
void GXTexCoord2s16(s16 s, s16 t);
extern u32 gc_gx_texcoord2s16_s, gc_gx_texcoord2s16_t;
static inline u32 rotl1(u32 v){return (v<<1)|(v>>31);} static inline void seed(u32 s){gc_gx_texcoord2s16_s=s^0x13579BDFu;gc_gx_texcoord2s16_t=s^0x2468ACE0u;}
static inline u32 h(void){u32 x=0; x=rotl1(x)^gc_gx_texcoord2s16_s; x=rotl1(x)^gc_gx_texcoord2s16_t; return x;} static inline void dump(uint8_t*o,u32*off){wr32be(o+(*off)*4,gc_gx_texcoord2s16_s);(*off)++;wr32be(o+(*off)*4,gc_gx_texcoord2s16_t);(*off)++;}
static u32 rs; static u32 rn(void){rs^=rs<<13;rs^=rs>>17;rs^=rs<<5;return rs;}
enum { L0 = 8, L1 = 16, L2 = 4, L3 = 2048, L4 = 4, L5 = 4 };

const char *gc_scenario_label(void) {
    return "GXTexCoord2s16/pbt_001";
}

const char *gc_scenario_out_path(void) {
    return "../actual/gx_tex_coord_2s16_pbt_001.bin";
}
void gc_scenario_run(GcRam *ram){uint8_t*out=gc_ram_ptr(ram,0x80300000u,0x6C); if(!out)die("gc_ram_ptr failed"); u32 off=0,tc=0,m=0,a=0,b=0,c=0,d=0,e=0,f=0;
 seed(0);GXTexCoord2s16(0,0);a=rotl1(a)^h(); seed(1);GXTexCoord2s16(1,-1);a=rotl1(a)^h(); seed(2);GXTexCoord2s16(32767,-32768);a=rotl1(a)^h(); seed(3);GXTexCoord2s16(-1024,1023);a=rotl1(a)^h();
 seed(4);GXTexCoord2s16((s16)0x00FF,(s16)0xFF00);a=rotl1(a)^h(); seed(5);GXTexCoord2s16((s16)0xFFFF,(s16)0x0001);a=rotl1(a)^h(); seed(6);GXTexCoord2s16((s16)0x8001,(s16)0x7F00);a=rotl1(a)^h(); seed(7);GXTexCoord2s16((s16)0x1111,(s16)0xEEEE);a=rotl1(a)^h(); tc+=L0; m=rotl1(m)^a;
 for(u32 i=0;i<4;i++)for(u32 j=0;j<4;j++){seed(0x100u+(i<<4)+j);GXTexCoord2s16((s16)(i*3000-(s16)(j*500)),(s16)(j*2500-(s16)(i*700)));b=rotl1(b)^h();} tc+=L1; m=rotl1(m)^b;
 for(u32 i=0;i<4;i++){seed(0x200u+i);s16 s=(s16)(0x1111u*(i+1u));s16 t=(s16)(0x2222u*(i+1u));GXTexCoord2s16(s,t);GXTexCoord2s16(s,t);c=rotl1(c)^h();} tc+=L2; m=rotl1(m)^c;
 rs=0xA1B2C3D4u; for(u32 i=0;i<L3;i++){seed(rn());GXTexCoord2s16((s16)rn(),(s16)rn());d=rotl1(d)^h();} tc+=L3; m=rotl1(m)^d;
 seed(0);GXTexCoord2s16(123,-456);e=rotl1(e)^h(); GXTexCoord2s16(-321,654);e=rotl1(e)^h(); GXTexCoord2s16((s16)0x00FF,(s16)0xFF00);e=rotl1(e)^h(); GXTexCoord2s16((s16)0x7FFF,(s16)0x8000);e=rotl1(e)^h(); tc+=L4; m=rotl1(m)^e;
 seed(0x33333333u);GXTexCoord2s16(0,-1);f=rotl1(f)^h(); seed(0x44444444u);GXTexCoord2s16((s16)0x7FFF,(s16)0x8000);f=rotl1(f)^h(); seed(0x55555555u);GXTexCoord2s16((s16)0x00FF,(s16)0xFF00);f=rotl1(f)^h(); seed(0x66666666u);GXTexCoord2s16((s16)0x0100,(s16)0x8001);f=rotl1(f)^h(); tc+=L5; m=rotl1(m)^f;
 wr32be(out+off*4,0x47535432u);off++;wr32be(out+off*4,tc);off++;wr32be(out+off*4,m);off++;wr32be(out+off*4,a);off++;wr32be(out+off*4,L0);off++;wr32be(out+off*4,b);off++;wr32be(out+off*4,L1);off++;wr32be(out+off*4,c);off++;wr32be(out+off*4,L2);off++;wr32be(out+off*4,d);off++;wr32be(out+off*4,L3);off++;wr32be(out+off*4,e);off++;wr32be(out+off*4,L4);off++;wr32be(out+off*4,f);off++;wr32be(out+off*4,L5);off++;
 seed(0);GXTexCoord2s16(123,-456);dump(out,&off); seed(0);GXTexCoord2s16(-321,654);dump(out,&off); seed(0);GXTexCoord2s16((s16)0x00FF,(s16)0xFF00);dump(out,&off); seed(0);GXTexCoord2s16((s16)0x7FFF,(s16)0x8000);dump(out,&off);
}
