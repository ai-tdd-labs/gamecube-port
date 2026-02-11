/*
 * card_checksum_strict_oracle.h
 *
 * STRICT_DECOMP oracle for __CARDCheckSum.
 * Body is copied from MP4 decomp:
 *   decomp_mario_party_4/src/dolphin/card/CARDCheck.c
 * (function __CARDCheckSum)
 */
#pragma once

#include <stdint.h>

#define ORACLE_STRICT_TIER "STRICT_DECOMP"

typedef uint16_t strict_u16;

static void strict_CARDCheckSum(void *ptr, int length,
                                strict_u16 *checksum, strict_u16 *checksumInv)
{
    strict_u16 *p;
    int i;

    length /= sizeof(strict_u16);
    *checksum = *checksumInv = 0;
    for (i = 0, p = ptr; i < length; i++, p++) {
        *checksum += *p;
        *checksumInv += ~*p;
    }
    if (*checksum == 0xffff) {
        *checksum = 0;
    }
    if (*checksumInv == 0xffff) {
        *checksumInv = 0;
    }
}
