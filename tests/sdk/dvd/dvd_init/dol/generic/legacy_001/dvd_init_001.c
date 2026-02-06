#ifdef GC_HOST_TEST
#include <stdint.h>
#include "runtime/runtime.h"
#include "runtime/dvd.h"
extern uint8_t *gc_test_ram;
#define RESULT_PTR() ((volatile uint8_t *)(gc_test_ram + 0x00300000))
#define TEST_OS_INIT() gc_runtime_init()
#define TEST_DVD_INIT() DVDInit()
#define TEST_DVD_STATUS() DVDGetDriveStatus()
typedef uint32_t u32;
static inline void store_be32(volatile uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}
#else
#include <stdint.h>
typedef uint32_t u32;
#define RESULT_PTR() ((volatile uint32_t *)0x80300000)

// Legacy synthetic implementation: enough to validate harness plumbing.
static u32 s_dvd_initialized;
static u32 s_drive_status;

static void TEST_OS_INIT(void) {}

static void DVDInit(void) {
    s_dvd_initialized = 1;
    // Pick a stable, non-zero status so we catch "always zero" bugs.
    s_drive_status = 0x12345678u;
}

static u32 DVDGetDriveStatus(void) {
    return s_drive_status;
}

#define TEST_DVD_INIT() DVDInit()
#define TEST_DVD_STATUS() DVDGetDriveStatus()
#endif

int main(void) {
    volatile u32* result = (volatile u32*)RESULT_PTR();

    // Setup
    TEST_OS_INIT();

    // Test
    TEST_DVD_INIT();

    // Write results
#ifdef GC_HOST_TEST
    store_be32((volatile uint8_t *)RESULT_PTR(), 0xDEADBEEF);
    store_be32((volatile uint8_t *)RESULT_PTR() + 4, (uint32_t)TEST_DVD_STATUS());
#else
    result[0] = 0xDEADBEEF;
    result[1] = (u32)TEST_DVD_STATUS();
#endif
#ifndef GC_HOST_TEST
    // Infinite loop
    while (1) {
    }
#endif

    return 0;
}
