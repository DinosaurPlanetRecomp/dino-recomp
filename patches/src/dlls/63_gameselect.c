#include "patches.h"
#include "rt64_extended_gbi.h"

#include "PR/ultratypes.h"
#include "PR/gbi.h"
#include "dlls/engine/63_gameselect.h"
#include "sys/memory.h"
#include "sys/fonts.h"
#include "sys/rcp.h"
#include "dll.h"

// @recomp: This file patches out the partial/conditional redraw behavior of the 
//          menu as it bugs out RT64 when in higher resolutions and/or widescreen. 

#include "recomp/dlls/engine/63_gameselect_recomp.h"

extern GameSelectSubmenu sSubmenus[];
extern GameTextChunk *sGameTextChunk;
extern s8 sSubmenuIdx;
extern s8 sSelectedSaveIdx;

extern GameSelectSaveInfo sSaveGameInfo[3];
extern s8 sExitTransitionTimer;
extern s8 sRedrawFrames;
extern s16 sSaveGameBoxX;
extern s16 sSaveGameBoxY;
extern u8 sExitToGame;
extern u8 sExitToMainMenu;
extern Texture *sBackgroundTexture;
extern Texture *sLogoTexture;
extern Texture *sLogoShadowTexture;
extern char sRecentTaskNumStrs[4][4];

extern void dll_63_draw_save_game_box(Gfx **gdl, s32 x, s32 y, GameSelectSaveInfo *saveInfo);

static char *recent_task_strs[3];
static s32 num_recent_task_strs = 0;

static void free_recent_task_strs() {
    for (s32 i = 0; i < num_recent_task_strs; i++) {
        mmFree(recent_task_strs[i]);
        recent_task_strs[i] = NULL;
    }

    num_recent_task_strs = 0;
}

RECOMP_PATCH void dll_63_dtor(void *self) {
    // @recomp: Free the task strings we manage in the patch
    free_recent_task_strs();
}

RECOMP_PATCH void dll_63_draw(Gfx **gdl, Mtx **mtxs, Vertex **vtxs) {
    s32 numRecentTasks;
    s32 uly;
    s32 lry;
    s32 ulx;
    s32 lrx;
    s32 y;
    s32 i;
    GameSelectSubmenu *submenu;

    submenu = &sSubmenus[sSubmenuIdx];

    // @recomp: Always redraw
    sRedrawFrames = 100;

    if ((sExitToGame || sExitToMainMenu) && (sExitTransitionTimer <= 10)) {
        return;
    }
    
    fontWindowSetCoords(1, 0, 0,
        GET_VIDEO_WIDTH(viGetCurrentSize()) - 100,
        GET_VIDEO_HEIGHT(viGetCurrentSize()));
    fontWindowFlushStrings(1);

    fontWindowSetCoords(3, 105, 0,
        GET_VIDEO_WIDTH(viGetCurrentSize()) - 200,
        GET_VIDEO_HEIGHT(viGetCurrentSize()));
    fontWindowFlushStrings(3);

    if (sRedrawFrames != 0) {
        // @recomp: Center background
        // TODO: the clear screen is only necessary because coming from the rolling demo, some 3d stuff still draws??
        rcpClearScreen(gdl, mtxs, CLEAR_COLOR);
        gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, (-640 / 2) * 4, 0, (640 / 2) * 4, 0);
        rcpScreenFullWrite(gdl, sBackgroundTexture, 0, 0, 0, 0, 0xFF, SCREEN_WRITE_CYC_COPY);
        gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);

        if (sSubmenuIdx == SUBMENU_GAME_RECAP) {
            rcpScreenFullWrite(gdl, sLogoShadowTexture, 119, 92, 0, 0, 0xFF, SCREEN_WRITE_TRANSLUCENT);
            rcpScreenFullWrite(gdl, sLogoTexture, 129, 100, 0, 0, 0xFF, SCREEN_WRITE_TRANSLUCENT);

            numRecentTasks = gDLL_30_Task->vtbl->get_num_recently_completed();
            if (numRecentTasks > 3) {
                numRecentTasks = 3;
            }

            fontWindowEnableWordwrap(3);
            fontWindowUseFont(1, FONT_FUN_FONT);
            fontWindowUseFont(3, FONT_FUN_FONT);
            fontWindowSetTextColour(1, 183, 139, 97, 255, 255);
            fontWindowSetTextColour(3, 183, 139, 97, 255, 255);

            // @recomp: Fix memory leak with task strings
            free_recent_task_strs();
            for (i = 0; i < numRecentTasks; i++) {
                recent_task_strs[i] = gDLL_30_Task->vtbl->get_recently_completed_task_text(i);
            }
            num_recent_task_strs = numRecentTasks;

            y = 232;
            for (i = 0; i < numRecentTasks; i++) {
                sprintf(sRecentTaskNumStrs[i], "%1d.", (int)(i + 1));
                fontWindowAddStringXY(1, 75, y, sRecentTaskNumStrs[i], 1, ALIGN_TOP_LEFT);
                fontWindowAddStringXY(3, 2, y, recent_task_strs[i], 1, ALIGN_TOP_LEFT);
                y += 40;
            }

            // @recomp: Fix text wrap for dropshadow (it can wrap differently if the window size isn't adjusted due to the offset)
            fontWindowSetCoords(1, 0, 0, 
                GET_VIDEO_WIDTH(viGetCurrentSize()) - 100 - 2, 
                GET_VIDEO_HEIGHT(viGetCurrentSize()) - 2);
            fontWindowSetCoords(3, 105, 0, 
                GET_VIDEO_WIDTH(viGetCurrentSize()) - 200 - 2, 
                GET_VIDEO_HEIGHT(viGetCurrentSize()) - 2);

            y = 232;
            fontWindowSetTextColour(1, 0, 0, 0, 255, 255);
            fontWindowSetTextColour(3, 0, 0, 0, 255, 255);
            for (i = 0; i < numRecentTasks; i++) {
                sprintf(sRecentTaskNumStrs[i], "%1d.", (int)(i + 1));
                fontWindowAddStringXY(1, 73, y - 2, sRecentTaskNumStrs[i], 1, ALIGN_TOP_LEFT);
                fontWindowAddStringXY(3, 0, y - 2, recent_task_strs[i], 1, ALIGN_TOP_LEFT);
                y += 40;
            }
        } else {
            if (sSelectedSaveIdx != -1) {
                dll_63_draw_save_game_box(gdl, sSaveGameBoxX, sSaveGameBoxY, &sSaveGameInfo[sSelectedSaveIdx]);
            }

            fontWindowUseFont(1, FONT_FUN_FONT);
            fontWindowSetTextColour(1, 183, 139, 97, 255, 255);

            fontWindowAddStringXY(1, 320, 405, sGameTextChunk->strings[submenu->buttonLegendTextIdx], 1, ALIGN_TOP_CENTER);
            fontWindowSetTextColour(1, 0, 0, 0, 255, 255);
            fontWindowAddStringXY(1, 318, 403, sGameTextChunk->strings[submenu->buttonLegendTextIdx], 2, ALIGN_TOP_CENTER);
        }

        fontWindowSetCoords(2, 0, 0, 
            GET_VIDEO_WIDTH(viGetCurrentSize()) - 100, 
            GET_VIDEO_HEIGHT(viGetCurrentSize()));
        fontWindowFlushStrings(2);
        fontWindowUseFont(2, FONT_DINO_MEDIUM_FONT_IN);
        fontWindowEnableWordwrap(2);
        fontWindowSetTextColour(2, 255, 255, 255, 0, 255);

        fontWindowAddStringXY(2, 69, 61, sGameTextChunk->strings[submenu->titleTextIdx], 1, ALIGN_TOP_LEFT);
        fontWindowSetTextColour(2, 0, 0, 0, 255, 255);
        fontWindowAddStringXY(2, 64, 56, sGameTextChunk->strings[submenu->titleTextIdx], 2, ALIGN_TOP_LEFT);

        fontWindowDraw(gdl, NULL, NULL, 2);
    } else {
        // Always redraw background in case picmenu redraws
        menu_func_80010158(&ulx, &lrx, &uly, &lry);
        rcpScreenScrollWrite(gdl, sBackgroundTexture, 0, 0, uly, lry, 0xFF, SCREEN_WRITE_CYC_COPY);
    }

    // @recomp: Always redraw all
    gDLL_74_Picmenu->vtbl->redraw_all();
    gDLL_74_Picmenu->vtbl->draw(gdl);

    fontWindowDraw(gdl, NULL, NULL, 1);
    fontWindowDraw(gdl, NULL, NULL, 3);

    sRedrawFrames -= 1;
    if (sRedrawFrames < 0) {
        sRedrawFrames = 0;
    }
}
