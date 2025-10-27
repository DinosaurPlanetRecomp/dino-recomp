#include "patches.h"

#include "PR/os.h"
#include "game/gamebits.h"
#include "sys/controller.h"
#include "sys/main.h"
#include "sys/menu.h"
#include "dll.h"

static s32 should_skip_splash() {
    return (get_button_presses(0) & A_BUTTON) != 0;
}


#include "recomp/dlls/engine/60_post_recomp.h"

extern u8 data_0;
extern u8 data_4;

extern f32 bss_0;
extern s8 bss_4;
extern s8 bss_6;
extern f32 bss_8;
extern f32 bss_C;
extern f32 bss_10;

RECOMP_PATCH s32 dll_60_update1() {
    if (data_4 == 1) {
        return 0;
    } else {
        bss_0 += gUpdateRateF;

        // @recomp: Allow skipping
        if (should_skip_splash()) {
            // Fade in background if we haven't shown it yet
            if (bss_0 <= 240.0f) {
                gDLL_28_ScreenFade->vtbl->fade_reversed(30, SCREEN_FADE_BLACK);
            }
            // Let 1 frame render first so the background shows up for the next menu (we render it)
            bss_0 = 720.0f - 0.01f;
            bss_6 = 2;
        }

        if (bss_6 == 0 && bss_0 > 205.0f) {
            gDLL_28_ScreenFade->vtbl->fade(30, SCREEN_FADE_BLACK);
            bss_6 = 1;
        } else if (bss_6 == 1 && bss_0 > 245.0f) {
            gDLL_28_ScreenFade->vtbl->fade_reversed(30, SCREEN_FADE_BLACK);
            bss_6 = 2;
        }

        if (data_0 > 1) {
            bss_8 -= gUpdateRateF;
        }
        if (data_0 > 2) {
            bss_C -= gUpdateRateF;
        }
        if (data_0 > 3) {
            bss_10 -= gUpdateRateF;
        }

        if (bss_0 > 720.0f) {
            bss_4 = 1;
        }

        if (bss_4 != 0) {
            bss_4 = 0;
            bss_0 = 0.0f;
            menu_set(MENU_RAREWARE);
        }

        return 0;
    } 
}


#include "recomp/dlls/engine/61_rareware_recomp.h"

extern s32 data_0;
extern s8 data_4;

extern s8 bss_0;
extern s8 bss_1;
extern s8 bss_2;
extern f32 bss_4;
extern f32 bss_8;

RECOMP_PATCH s32 dll_61_update1() {
    s32 delay;

    delay = gUpdateRate;
    if (delay > 3) {
        delay = 3;
    }

    if (bss_1 > 0) {
        bss_1 -= delay;
    }

    // @recomp: Allow skipping
    if (bss_2 != 0 || should_skip_splash()) {
        main_set_bits(BIT_44F, 0);
        menu_set(MENU_GAME_SELECT);
    }

    data_0 += gUpdateRate;
    if (data_0 > 620) {
        bss_0 = 1;
    }

    if (bss_0 != 0) {
        gDLL_28_ScreenFade->vtbl->fade(30, SCREEN_FADE_BLACK);
        bss_1 = 45;
        bss_2 = 1;
    }

    if (data_4 > 0) {
        bss_4 -= gUpdateRateF;
    }
    if (data_4 > 2) {
        bss_8 -= gUpdateRateF;
    }

    return 0;
}
