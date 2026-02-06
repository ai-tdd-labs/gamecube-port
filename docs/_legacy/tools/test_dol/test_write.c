/*
 * Simple test DOL for RAM dump verification
 *
 * Writes a known pattern to 0x80300000 and loops forever.
 * Used to verify ram_dump.py works correctly.
 *
 * Minimal version - no libogc calls except cache flush.
 */

#include <gccore.h>

// Test data location - safe area in RAM
#define TEST_ADDR 0x80300000
#define TEST_SIZE 256

int main(void) {
    volatile unsigned int *ptr = (volatile unsigned int *)TEST_ADDR;
    volatile unsigned char *byte_ptr;
    int i;

    // First 64 bytes: 0xDEADBEEF repeated (16 times)
    for (i = 0; i < 16; i++) {
        ptr[i] = 0xDEADBEEF;
    }

    // Next 64 bytes: 0xCAFEBABE repeated
    for (i = 16; i < 32; i++) {
        ptr[i] = 0xCAFEBABE;
    }

    // Next 128 bytes: sequential 0x00 to 0x7F
    byte_ptr = (volatile unsigned char *)(TEST_ADDR + 128);
    for (i = 0; i < 128; i++) {
        byte_ptr[i] = (unsigned char)i;
    }

    // Flush data cache to ensure writes are visible
    DCFlushRange((void *)TEST_ADDR, TEST_SIZE);

    // Infinite loop - keeps GDB stub alive
    while (1) {
        // Tight loop
    }

    return 0;
}
