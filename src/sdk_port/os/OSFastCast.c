#include <stdint.h>

// MP4 calls OSInitFastCast() once during init. The real SDK implementation is a
// Metrowerks-only inline that programs GQR2-5 for paired-single quantization.
// Our host tests only require that the symbol exists and is safe to call.
void OSInitFastCast(void) {}
