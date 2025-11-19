#include "patches.h"

#include "PR/gbi.h"
#include "PR/ultratypes.h"
#include "dlls/engine/21_gametext.h"
#include "dlls/engine/74_picmenu.h"
#include "sys/fonts.h"
#include "sys/gfx/gx.h"
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
    
    font_window_set_coords(1, 0, 0, 
        GET_VIDEO_WIDTH(vi_get_current_size()), 
        GET_VIDEO_HEIGHT(vi_get_current_size()));
    font_window_flush_strings(1);
    font_window_use_font(1, FONT_DINO_MEDIUM_FONT_IN);

    if (sMainRedrawFrames != 0) {
        func_8003825C(gdl, sBackgroundTexture, 0, 0, 0, 0, 0xFF, 2);
        
        font_window_set_text_colour(1, 255, 255, 255, 0, 255);
        font_window_add_string_xy(1, 320, 73, sGameTextChunk->strings[0x1E], 1, ALIGN_TOP_CENTER);

        font_window_set_text_colour(1, 0, 0, 0, 255, 255);
        font_window_add_string_xy(1, 315, 68, sGameTextChunk->strings[0x1E], 2, ALIGN_TOP_CENTER);

        font_window_use_font(1, FONT_FUN_FONT);
        font_window_set_text_colour(1, 183, 139, 97, 255, 255);

        font_window_add_string_xy(1, 320, 405, sGameTextChunk->strings[0x1F], 1, ALIGN_TOP_CENTER);
        font_window_set_text_colour(1, 0, 0, 0, 255, 255);
        font_window_add_string_xy(1, 318, 403, sGameTextChunk->strings[0x1F], 2, ALIGN_TOP_CENTER);
    } else {
        // Always redraw background in case picmenu redraws
        func_80010158(&ulx, &lrx, &uly, &lry);
        func_800382AC(gdl, sBackgroundTexture, 0, 0, uly, lry, 0xFF, 2);
    }

    // @recomp: Always redraw all
    gDLL_74_Picmenu->vtbl->redraw_all();
    gDLL_74_Picmenu->vtbl->draw(gdl);

    if (sNameLettersRedrawFrames != 0) {
        if (sMainRedrawFrames == 0) {
            // Make sure we at least redraw the background behind the name letters
            lry = (sLetterBgBoxTexture->height | ((sLetterBgBoxTexture->unk1B & 0xF) << 8));
            func_800382AC(gdl, sBackgroundTexture, 0, 0, 110, lry + 110, 0xFF, 2);
        }

        dll_64_draw_letters(gdl, 179, 110);
    }

    font_window_draw(gdl, NULL, NULL, 1);

    sMainRedrawFrames -= 1;
    if (sMainRedrawFrames < 0) {
        sMainRedrawFrames = 0;
    }

    sNameLettersRedrawFrames -= 1;
    if (sNameLettersRedrawFrames < 0) {
        sNameLettersRedrawFrames = 0;
    }
}
