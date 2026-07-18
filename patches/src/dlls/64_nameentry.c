#include "patches.h"

#include "PR/gbi.h"
#include "PR/ultratypes.h"
#include "dlls/engine/21_gametext.h"
#include "dlls/engine/74_picmenu.h"
#include "sys/fonts.h"
#include "sys/gfx/texture.h"
#include "sys/menu.h"
#include "sys/rcp.h"
#include "dll.h"
#include "types.h"

// @recomp: This file patches out the partial/conditional redraw behavior of the 
//          menu as it bugs out RT64 when in higher resolutions and/or widescreen. 

#include "recomp/dlls/engine/64_nameentry_recomp.h"

extern GameTextChunk *sGameTextChunk;

extern Texture *sLetterBgBoxTexture;
extern s8 sMainRedrawFrames;
extern s8 sNameLettersRedrawFrames;
extern Texture *sBackgroundTexture;

extern void dll_64_draw_letters(Gfx **gdl, s32 x, s32 y);

RECOMP_PATCH void dll_64_draw(Gfx **gdl, Mtx **mtxs, Vertex **vtxs) {
    s32 ulx;
    s32 lrx;
    s32 uly;
    s32 lry;

    // @recomp: Always redraw
    sMainRedrawFrames = 100;
    sNameLettersRedrawFrames = 100;
    
    fontWindowSetCoords(1, 0, 0, 
        GET_VIDEO_WIDTH(viGetCurrentSize()), 
        GET_VIDEO_HEIGHT(viGetCurrentSize()));
    fontWindowFlushStrings(1);
    fontWindowUseFont(1, FONT_DINO_MEDIUM_FONT_IN);

    if (sMainRedrawFrames != 0) {
        // @recomp: Center background for widescreen
        gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, (-640 / 2) * 4, 0, (640 / 2) * 4, 0);
        rcpScreenFullWrite(gdl, sBackgroundTexture, 0, 0, 0, 0, 0xFF, SCREEN_WRITE_CYC_COPY);
        gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
        
        fontWindowSetTextColour(1, 255, 255, 255, 0, 255);
        fontWindowAddStringXY(1, 320, 73, sGameTextChunk->strings[0x1E], 1, ALIGN_TOP_CENTER);

        fontWindowSetTextColour(1, 0, 0, 0, 255, 255);
        fontWindowAddStringXY(1, 315, 68, sGameTextChunk->strings[0x1E], 2, ALIGN_TOP_CENTER);

        fontWindowUseFont(1, FONT_FUN_FONT);
        fontWindowSetTextColour(1, 183, 139, 97, 255, 255);

        fontWindowAddStringXY(1, 320, 405, sGameTextChunk->strings[0x1F], 1, ALIGN_TOP_CENTER);
        fontWindowSetTextColour(1, 0, 0, 0, 255, 255);
        fontWindowAddStringXY(1, 318, 403, sGameTextChunk->strings[0x1F], 2, ALIGN_TOP_CENTER);
    } else {
        // Always redraw background in case picmenu redraws
        menu_func_80010158(&ulx, &lrx, &uly, &lry);
        rcpScreenScrollWrite(gdl, sBackgroundTexture, 0, 0, uly, lry, 0xFF, SCREEN_WRITE_CYC_COPY);
    }

    // @recomp: Always redraw all
    gDLL_74_Picmenu->vtbl->redraw_all();
    gDLL_74_Picmenu->vtbl->draw(gdl);

    if (sNameLettersRedrawFrames != 0) {
        if (sMainRedrawFrames == 0) {
            // Make sure we at least redraw the background behind the name letters
            lry = (sLetterBgBoxTexture->height | ((sLetterBgBoxTexture->widthHeightHi & 0xF) << 8));
            rcpScreenScrollWrite(gdl, sBackgroundTexture, 0, 0, 110, lry + 110, 0xFF, SCREEN_WRITE_CYC_COPY);
        }

        dll_64_draw_letters(gdl, 179, 110);
    }

    fontWindowDraw(gdl, NULL, NULL, 1);

    sMainRedrawFrames -= 1;
    if (sMainRedrawFrames < 0) {
        sMainRedrawFrames = 0;
    }

    sNameLettersRedrawFrames -= 1;
    if (sNameLettersRedrawFrames < 0) {
        sNameLettersRedrawFrames = 0;
    }
}
