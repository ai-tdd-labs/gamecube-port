#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_mem[10] = {101,102,103,104,105,106,107,108,109,110};

static void GXReadMemMetric(u32 *a,u32 *b,u32 *c,u32 *d,u32 *e,u32 *f,u32 *g,u32 *h,u32 *i,u32 *j) {
    if (a) *a = s_mem[0];
    if (b) *b = s_mem[1];
    if (c) *c = s_mem[2];
    if (d) *d = s_mem[3];
    if (e) *e = s_mem[4];
    if (f) *f = s_mem[5];
    if (g) *g = s_mem[6];
    if (h) *h = s_mem[7];
    if (i) *i = s_mem[8];
    if (j) *j = s_mem[9];
}

int main(void) {
    u32 a=0,b=0,c=0,d=0,e=0,f=0,g=0,h=0,i=0,j=0;
    GXReadMemMetric(&a,&b,&c,&d,&e,&f,&g,&h,&i,&j);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = a;
    *(volatile u32*)0x80300008 = b;
    *(volatile u32*)0x8030000C = c;
    *(volatile u32*)0x80300010 = d;
    *(volatile u32*)0x80300014 = e;
    *(volatile u32*)0x80300018 = f;
    *(volatile u32*)0x8030001C = g;
    *(volatile u32*)0x80300020 = h;
    *(volatile u32*)0x80300024 = i;
    *(volatile u32*)0x80300028 = j;
    while (1) {}
}
