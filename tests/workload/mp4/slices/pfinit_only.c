#include "game/printfunc.h"

// MP4 decomp slice: `pfInit` + `pfClsScr` from
// `decomp_mario_party_4/src/game/printfunc.c`.
//
// Reachability-only: this is here so MP4 main() init can progress on host.

typedef struct {
    unsigned short type;
    short empstrline_next;
    char str[80];
} StrLine;

static StrLine strline[256];

int fontcolor;
unsigned short empstrline;
unsigned short strlinecnt;

void pfInit(void) {
    fontcolor = 15;
    empstrline = 0;

    for (int i = 0; i < 256; i++) {
        strline[i].str[0] = 0;
    }
    pfClsScr();
}

void pfClsScr(void) {
    empstrline = 0;
    strlinecnt = 0;
    for (int i = 0; i < 256; i++) {
        strline[i].empstrline_next = (short)(i + 1);
        strline[i].type = 0;
        if (strline[i].str[0] != 0) {
            strline[i].str[0] = 0;
        }
    }
}

