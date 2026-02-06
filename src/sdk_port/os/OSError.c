#include <stdint.h>
#include <stdarg.h>

// Deterministic host-side stubs. These are intentionally non-halting so tests
// can compare memory output.

uint32_t gc_os_panic_calls;

int OSReport(const char *msg, ...) {
    (void)msg;
    return 0;
}

void OSPanic(const char *file, int line, const char *msg, ...) {
    (void)file;
    (void)line;
    (void)msg;
    gc_os_panic_calls++;
}
