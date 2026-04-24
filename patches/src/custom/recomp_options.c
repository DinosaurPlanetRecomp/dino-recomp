#include "recomp_funcs.h"
#include "recomp_options.h"
#include "patches/mp3.h"
#include "patches/1_cmdmenu.h"

#include "dlls/engine/29_gplay.h"
#include "game/gamebits.h"
#include "sys/main.h"
#include "dll.h"

enum RecompHUDMode {
    RECOMP_HUD_Default = 0,
    RECOMP_HUD_AlwaysVisible = 1
};

enum RecompMinimapMode {
    RECOMP_MINIMAP_Default = 0, // always visible
    RECOMP_MINIMAP_Hold = 1, // if R is held for cmdmenu
    RECOMP_MINIMAP_Hidden = 2
};

static s32 recomp_lastDialogVolume = 100;
static s32 recomp_lastHudMode = -1;
static s32 recomp_lastMinimapMode = -1;

static f32 recomp_holdMinimapTimer = 0;

void recomp_pull_game_options(void) {
    static s32 b_firstCall = TRUE;

    if (b_firstCall) {
        gDLL_29_Gplay->vtbl->load_game_options();
    }
    GplayOptions *options = gDLL_29_Gplay->vtbl->get_game_options();

    s32 recompMusicVol = (recomp_get_bgm_volume() / 100.0f) * 256;
    if (options->volumeMusic != recompMusicVol || b_firstCall) {
        options->volumeMusic = recompMusicVol;
        gDLL_5_AMSEQ2->vtbl->set_volume_option(recompMusicVol);
    }

    s32 recompSfxVol = (recomp_get_sfx_volume() / 100.0f) * 127;
    if (options->volumeAudio != recompSfxVol || b_firstCall) {
        options->volumeAudio = recompSfxVol;
        gDLL_6_AMSFX->vtbl->func_7E4(recompSfxVol);
    }

    s32 recompSubtitles = recomp_get_subtitles_enabled();
    if (options->showSubtitles != recompSubtitles || b_firstCall) {
        options->showSubtitles = recompSubtitles;
        gDLL_22_Subtitles->vtbl->func_2D0(recompSubtitles);
    }

    s32 recompDialogVol = recomp_get_dialog_volume();
    if (recomp_lastDialogVolume != recompDialogVol || b_firstCall) {
        recomp_lastDialogVolume = recompDialogVol;
        recomp_update_mp3_volume();
    }

    s32 recompHudMode = recomp_get_hud_mode();
    if (recomp_lastHudMode != recompHudMode || b_firstCall) {
        recomp_lastHudMode = recompHudMode;
        gDLL_1_cmdmenu->vtbl->toggle_forced_stats_display(recompHudMode == RECOMP_HUD_AlwaysVisible ? 1 : 0);    
    }

    s32 recompMinimapMode = recomp_get_minimap_mode();
    if (recomp_lastMinimapMode != recompMinimapMode || b_firstCall) {
        recomp_lastMinimapMode = recompMinimapMode;
        main_set_bits(BIT_Hide_Minimap, recompMinimapMode == RECOMP_MINIMAP_Hidden ? 1 : 0);
        recomp_holdMinimapTimer = 0.0f;
    }
    
    if (recompMinimapMode == RECOMP_MINIMAP_Hold) {
        recomp_holdMinimapTimer -= gUpdateRateF;
        if (recomp_cmdmenu_is_r_held()) {
            recomp_holdMinimapTimer = 90.0f;
        }
        main_set_bits(BIT_Hide_Minimap, recomp_holdMinimapTimer > 0.0f ? 0 : 1);
    }

    b_firstCall = FALSE;
}
