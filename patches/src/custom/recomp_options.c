#include "recomp_funcs.h"
#include "recomp_options.h"
#include "patches/mp3.h"

#include "dlls/engine/29_gplay.h"
#include "dll.h"

static s32 recomp_lastDialogVolume = 100;

void recomp_pull_game_options(void) {
    static s32 b_firstCall = TRUE;

    GplayOptions *options = gDLL_29_Gplay->vtbl->get_game_options();

    s32 recompMusicVol = (recomp_get_bgm_volume() / 100.0f) * 256;
    if (options->volumeMusic != recompMusicVol || b_firstCall) {
        options->volumeMusic = recompMusicVol;
        gDLL_5_AMSEQ2->vtbl->func9.withOneArg(recompMusicVol);
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

    b_firstCall = FALSE;
}
