#ifndef _DOLPHIN_OSCONTEXT
#define _DOLPHIN_OSCONTEXT

#include <dolphin/types.h>

typedef struct OSContext {
    u32 gpr[32];
} OSContext;

void OSClearContext(OSContext *context);
void OSSetCurrentContext(OSContext *context);

#endif // _DOLPHIN_OSCONTEXT
