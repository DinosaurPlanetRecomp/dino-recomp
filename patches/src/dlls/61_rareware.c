#include "patches.h"

#include "PR/os.h"
#include "game/gamebits.h"
#include "sys/joypad.h"
#include "sys/main.h"
#include "sys/menu.h"
#include "dll.h"

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
    if (bss_2 != 0 || (joy_get_pressed_raw(0) & A_BUTTON) != 0) {
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
