#include <stdint.h>
#include <stdio.h>

typedef int32_t s32;

typedef struct {
    volatile s32 state;
} DVDCommandBlock;

s32 DVDCancel(DVDCommandBlock *block);

static int g_failures;

static void expect_i32(const char *label, s32 got, s32 want) {
    if (got != want) {
        fprintf(stderr, "FAIL %s: got=%d want=%d\n", label, got, want);
        g_failures++;
    }
}

int main(void) {
    DVDCommandBlock b;

    expect_i32("null block", DVDCancel(NULL), -1);

    b.state = 0;
    expect_i32("state END ret", DVDCancel(&b), 0);
    expect_i32("state END keep", b.state, 0);

    b.state = -1;
    expect_i32("state FATAL ret", DVDCancel(&b), 0);
    expect_i32("state FATAL keep", b.state, -1);

    b.state = 10;
    expect_i32("state CANCELED ret", DVDCancel(&b), 0);
    expect_i32("state CANCELED keep", b.state, 10);

    b.state = 1;
    expect_i32("state BUSY ret", DVDCancel(&b), 0);
    expect_i32("state BUSY -> CANCELED", b.state, 10);

    b.state = 3;
    expect_i32("state WAIT ret", DVDCancel(&b), 0);
    expect_i32("state WAIT -> CANCELED", b.state, 10);

    if (g_failures != 0) {
        fprintf(stderr, "dvdcancel_unit_test: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("dvdcancel_unit_test: PASS\n");
    return 0;
}
