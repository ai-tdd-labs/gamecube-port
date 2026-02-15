#include <stdint.h>

#include "card_bios.h"
#include "card_mem.h"

/*
 * CARD directory helpers (host port).
 *
 * Mirrors the minimal behavior from decomp CARDDir.c without the EXI layer.
 */

struct CARDDir* __CARDGetDirBlock(struct GcCardControl *card)
{
    if (!card || !card->current_dir_ptr) {
        return 0;
    }
    // Directory is one system block (8KiB).
    return (struct CARDDir*)card_ptr(card->current_dir_ptr, 8u * 1024u);
}
