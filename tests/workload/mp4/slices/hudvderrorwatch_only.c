#include <stdint.h>

// Minimal MP4 decomp slice: HuDvdErrorWatch only.
//
// This is game-specific code, not part of the Nintendo SDK port. The purpose
// is reachability: keep the MP4 main loop workload closer to the decomp while
// still using sdk_port as the only implementation of SDK calls.

// sdk_port (DVD + OS)
int DVDGetDriveStatus(void);
void OSReport(const char *fmt, ...);

// decomp_mario_party_4/src/game/dvd.c
static int beforeDvdStatus = 0;

void HuDvdErrorWatch(void)
{
    int status = DVDGetDriveStatus();
    if (status == beforeDvdStatus) {
        return;
    }
    beforeDvdStatus = status;
    switch (status + 1) {
        case 0:
            OSReport("DVD ERROR:Fatal error occurred\n***HALT***");
            while (1) {
                // hang (matches decomp behavior)
            }
            break;

        case 5:
            OSReport("DVD ERROR:No disk\n");
            break;

        case 6:
            OSReport("DVD ERROR:Cover open\n");
            break;

        case 7:
            OSReport("DVD ERROR:Wrong disk\n");
            break;

        case 12:
            OSReport("DVD ERROR:Please retry\n");
            break;

        default:
            break;
    }
}

