#include <stdint.h>

// Minimal, host-buildable slice of MP4 decomp: HuSprInit from sprman.c.
// Purpose: reachability/integration only (not a correctness oracle).

#define HUSPR_MAX 384
#define HUSPR_GRP_MAX 256

typedef struct {
    // Only fields touched by HuSprInit.
    int16_t prio;
    void *data;
} HuSprite;

typedef struct {
    // Only fields touched by HuSprInit.
    int16_t capacity;
} HuSprGrp;

HuSprite HuSprData[HUSPR_MAX];
HuSprGrp HuSprGrpData[HUSPR_GRP_MAX];

static int HuSprPauseF;

void HuSprInit(void)
{
    int16_t i;
    HuSprite *sprite;
    HuSprGrp *group;

    for (sprite = &HuSprData[1], i = 1; i < HUSPR_MAX; i++, sprite++) {
        sprite->data = 0;
    }
    for (group = HuSprGrpData, i = 0; i < HUSPR_GRP_MAX; i++, group++) {
        group->capacity = 0;
    }

    sprite = &HuSprData[0];
    sprite->prio = 0;
    sprite->data = (void *)1;
    HuSprPauseF = 0;
}

