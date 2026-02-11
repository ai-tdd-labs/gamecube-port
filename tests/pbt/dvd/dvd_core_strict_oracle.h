/*
 * dvd_core_strict_oracle.h
 *
 * Strict leaf oracle for deterministic read-window semantics used by
 * DVDFastOpen/DVDRead/DVDReadAsync property checks.
 *
 * This models the leaf arithmetic constraints directly:
 * - invalid offset/len => fail
 * - clamp read to file end
 */
#pragma once

#include <stdint.h>

typedef struct {
    int32_t ok;        /* async-style success flag: 1 or 0 */
    int32_t n;         /* copied bytes, or -1 on invalid input */
    int32_t sync_ret;  /* sync-style return: n or -1 */
    int32_t prio_ret;  /* prio-style return: 0 or -1 */
} strict_dvd_read_window_t;

static strict_dvd_read_window_t strict_dvd_read_window(uint32_t file_len, int32_t off, int32_t len) {
    strict_dvd_read_window_t r;
    r.ok = 0;
    r.n = -1;
    r.sync_ret = -1;
    r.prio_ret = -1;

    if (off < 0 || len < 0 || (uint32_t)off > file_len) {
        return r;
    }

    uint32_t avail = file_len - (uint32_t)off;
    uint32_t want = (uint32_t)len;
    r.n = (int32_t)((want <= avail) ? want : avail);
    r.ok = 1;
    r.sync_ret = r.n;
    r.prio_ret = 0;
    return r;
}
