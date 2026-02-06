#include <stdint.h>

typedef uint32_t u32;

static volatile u32 s_pix[6] = {11,22,33,44,55,66};

static void GXReadPixMetric(u32 *a, u32 *b, u32 *c, u32 *d, u32 *e, u32 *f) {
    if (a) *a = s_pix[0];
    if (b) *b = s_pix[1];
    if (c) *c = s_pix[2];
    if (d) *d = s_pix[3];
    if (e) *e = s_pix[4];
    if (f) *f = s_pix[5];
}

int main(void) {
    u32 a=0,b=0,c=0,d=0,e=0,f=0;
    GXReadPixMetric(&a,&b,&c,&d,&e,&f);

    *(volatile u32*)0x80300000 = 0xDEADBEEFu;
    *(volatile u32*)0x80300004 = a;
    *(volatile u32*)0x80300008 = b;
    *(volatile u32*)0x8030000C = c;
    *(volatile u32*)0x80300010 = d;
    *(volatile u32*)0x80300014 = e;
    *(volatile u32*)0x80300018 = f;
    while (1) {}
}
