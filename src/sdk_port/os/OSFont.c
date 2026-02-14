#include <stdint.h>

typedef uint16_t u16;

// Deterministic knob for host tests.
u16 gc_os_font_encode;

u16 OSGetFontEncode(void)
{
    return gc_os_font_encode;
}

