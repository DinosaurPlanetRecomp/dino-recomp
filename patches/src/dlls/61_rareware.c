#include "patches.h"

#include "PR/os.h"
#include "game/gamebits.h"
#include "sys/joypad.h"
#include "sys/main.h"
#include "sys/menu.h"
#include "dll.h"

#include "recomp/dlls/engine/61_rareware_recomp.h"

#define KEYFRAME_40 40      //Logo fade-in start
#define KEYFRAME_50 50      //Logo faded in
#define KEYFRAME_285 285    //Logo glow start
#define KEYFRAME_620 620    //Screen fade to black

typedef enum {
    Rareware_STATE_Initial = 0,
    Rareware_STATE_Fading_In = 1,
    Rareware_STATE_Visible = 2,
    Rareware_STATE_Glowing = 3
} RarewareStates;

extern s32 dFrame;
extern s8 dState;

extern s8 sFadeOutStarted;
extern s8 sFadeOutTimer;
extern s8 sCutToNextScreen;
extern f32 sLogoTimer;
extern f32 sGlowTimer;

RECOMP_PATCH s32 rareware_update1() {
    s32 delay;

    //Get gUpdateRate, clamped at maximum of 3
    delay = gUpdateRate;
    if (delay > 3) {
        delay = 3;
    }

    //Decrement fadeout timer, if it's started
    if (sFadeOutTimer > 0) {
        sFadeOutTimer -= delay;
    }

    // @recomp: Allow skipping
    if (sCutToNextScreen != 0 || (joy_get_pressed_raw(0) & A_BUTTON) != 0) {
        main_set_bits(BIT_44F, 0);
        menu_set(MENU_GAME_SELECT);
    }

    dFrame += gUpdateRate;

    //Start fading out
    if (dFrame > KEYFRAME_620) {
        sFadeOutStarted = TRUE;
    }

    if (sFadeOutStarted) {
        gDLL_28_ScreenFade->vtbl->fade(30, SCREEN_FADE_BLACK);
        sFadeOutTimer = 45;
        sCutToNextScreen = TRUE; //@bug: cuts immediately, instead of at end of fade-out
    }

    if (dState >= Rareware_STATE_Fading_In) {
        sLogoTimer -= gUpdateRateF;
    }

    if (dState >= Rareware_STATE_Glowing) {
        sGlowTimer -= gUpdateRateF;
    }

    return 0;
}
