#include "game/gamework.h"
#include "game/flag.h"
#include "game/gamework_data.h"
#include "version.h"

#include <string.h>

// MP4 decomp slice: `GWInit` from `decomp_mario_party_4/src/game/gamework.c`.
//
// This slice exists to make the host workload harness compile+run along the
// real MP4 init order. It is reachability-only; it is not an oracle for SDK
// correctness.

s16 GwLanguage = 1;
s16 GwLanguageSave = -1;

GameStat GWGameStatDefault;
GameStat GWGameStat;
SystemState GWSystem;
PlayerState GWPlayer[4];
PlayerConfig GWPlayerCfg[4];

// Game-specific externs (implemented as stubs in the host scenario).
int HuPadStatGet(int pad_idx);
void GWRumbleSet(int enabled);
void GWMGExplainSet(int enabled);
void GWMGShowComSet(int enabled);
void GWMessSpeedSet(int speed);
void GWSaveModeSet(int mode);
void GWLanguageSet(s16 language);
void GWGameStatReset(void);

static inline void GWErase(void) {
    memset(GWPlayerCfg, 0, sizeof(GWPlayerCfg));
    memset(GWPlayer, 0, sizeof(GWPlayer));
    memset(&GWSystem, 0, sizeof(GWSystem));
}

static inline void InitPlayerConfig(void) {
    for (s32 i = 0; i < 4; i++) {
        PlayerConfig *config = &GWPlayerCfg[i];
        config->character = i;
        config->pad_idx = i;
        config->diff = 0;
        config->group = i;
        if (!HuPadStatGet(i)) {
            config->iscom = 0;
        } else {
            config->iscom = 1;
        }
    }
}

static inline void ResetBoardSettings(void) {
    GWRumbleSet(1);
    GWMGExplainSet(1);
    GWMGShowComSet(1);
    GWMessSpeedSet(1);
    GWSaveModeSet(0);
}

void GWInit(void) {
    GWGameStatReset();
    _InitFlag();
    GWErase();
    InitPlayerConfig();
#if VERSION_JP
    GWGameStat.language = 0;
#elif VERSION_ENG
    GWGameStat.language = 1;
#else
    GWLanguageSet(GwLanguage);
#endif
    ResetBoardSettings();
}

