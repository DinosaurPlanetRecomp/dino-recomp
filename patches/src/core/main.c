#include "builtin_dbgui.h"
#include "patches.h"
#include "patches/builtin_dbgui/graphics_window.h"
#include "patches/fbfx.h"
#include "patches/main.h"
#include "patches/rcp.h"
#include "dbgui.h"
#include "recomp_options.h"

#include "sys/gfx/gx.h"
#include "sys/gfx/map.h"
#include "sys/audio.h"
#include "sys/asset_thread.h"
#include "sys/dl_debug.h"
#include "sys/exception.h"
#include "sys/rcp.h"
#include "sys/rsp_segment.h"
#include "sys/main.h"
#include "sys/memory.h"
#include "sys/objects.h"
#include "sys/print.h"
#include "sys/voxmap.h"
#include "types.h"
#include "functions.h"
#include "dll.h"
#include "ui_funcs.h"

RECOMP_DECLARE_EVENT(recomp_on_game_tick_start());
RECOMP_DECLARE_EVENT(recomp_on_game_tick());
RECOMP_DECLARE_EVENT(recomp_on_dbgui());

extern Gfx *gMainGfx[2];
extern Gfx *gCurGfx;
extern Mtx *gMainMtx[2];
extern Mtx *gCurMtx;
extern Vertex *gMainVtx[2];
extern Vertex *gCurVtx;
extern Triangle *gMainPol[2];
extern Triangle *gCurPol;

extern u8 gFrameBufIdx;
extern s8 gPauseState;

extern void func_80013D80();
extern void func_80014074(void);
extern void NOTosSetTime(s32 arg0, s32 arg1);

// @recomp: Move graphics buffers into patch memory to save vanilla pool memory
static Gfx recompMainGfx[2][RECOMP_MAIN_GFX_BUF_SIZE / sizeof(Gfx)]; 
static Mtx recompMainMtx[2][RECOMP_MAIN_MTX_BUF_SIZE / sizeof(Mtx)]; 
static Vertex recompMainVtx[2][RECOMP_MAIN_VTX_BUF_SIZE / sizeof(Vertex)]; 
static Triangle recompMainPol[2][RECOMP_MAIN_POL_BUF_SIZE / sizeof(Triangle)]; 

RECOMP_PATCH void alloc_frame_buffers(void) {
    // @recomp: Use larger buffer sizes

    // in default.dol these have names as well.
    // alloc graphic display list command buffers. ("main:gfx" in default.dol)
    gMainGfx[0] = recompMainGfx[0];
    gMainGfx[1] = recompMainGfx[1];

    // matrix buffers ("main:mtx")
    gMainMtx[0] = recompMainMtx[0];
    gMainMtx[1] = recompMainMtx[1];

    // polygon buffers? ("main:pol")
    gMainPol[0] = recompMainPol[0];
    gMainPol[1] = recompMainPol[1];

    // vertex buffers ("main:vtx")
    gMainVtx[0] = recompMainVtx[0];
    gMainVtx[1] = recompMainVtx[1];
}

void recomp_dbgui_tick(void) {
    dbgui_ui_frame_begin();

    if (dbgui_is_open()) {
        builtin_dbgui();
        recomp_on_dbgui();

        if (dbgui_begin_main_menu_bar()) {
            dbgui_text("| Press ` or F9 to close debug UI.");
            dbgui_end_main_menu_bar();
        }
    }

    dbgui_ui_frame_end();
}

static void recomp_game_tick_start_hook(void) {
    recomp_on_game_tick_start();
    recomp_run_ui_callbacks();
    recomp_pull_game_options();
    recomp_dbgui_tick();
}

static void recomp_game_tick_hook(void) {
    recomp_on_game_tick();
    builtin_dbgui_game_tick();
}

RECOMP_PATCH void game_tick(void) {
    u8 phi_v1;
    u32 updateRate;
    Gfx **gdl;

    // @recomp: Do custom framebuffer FX implementation
    recomp_fbfx();

    osSetTime(0);
    dl_next_debug_info_set();

    gdl = &gCurGfx;

    // unused return type
    gfxtask_run_xbus(gMainGfx[gFrameBufIdx], gCurGfx, 0);

    gFrameBufIdx ^= 1;
    gCurGfx = gMainGfx[gFrameBufIdx];
    gCurMtx = gMainMtx[gFrameBufIdx];
    gCurVtx = gMainVtx[gFrameBufIdx];
    gCurPol = gMainPol[gFrameBufIdx];

    // @recomp: Enable RT64 extended GBI commands
    gEXEnable((*gdl)++);

    // @recomp: Hook start of game_tick
    recomp_game_tick_start_hook();

    dl_add_debug_info(gCurGfx, 0, "main/main.c", 0x28E);
    rsp_segment(&gCurGfx, SEGMENT_MAIN, (void *)K0BASE);
    rsp_segment(&gCurGfx, SEGMENT_FRAMEBUFFER, gFramebufferCurrent);
    rsp_segment(&gCurGfx, SEGMENT_ZBUFFER, D_800BCCB4);
    func_8003E9F0(&gCurGfx, gUpdateRate);
    dl_set_all_dirty();
    func_8003DB5C();

    if (gDLBuilder->needsPipeSync != 0) {
        gDLBuilder->needsPipeSync = 0;
        gDPPipeSync(gCurGfx++);
    }

    gDPSetDepthImage(gCurGfx++, SEGMENT_ZBUFFER << 24);

    rsp_init(&gCurGfx);
    phi_v1 = 2;

    if (func_80041D5C() == 0)
        phi_v1 = 0;
    else if (func_80041D74() == 0)
        phi_v1 = 3;

    func_80037A14(&gCurGfx, &gCurMtx, phi_v1);
    func_80007178();
    func_80013D80();
    audio_func_800121DC();
    gDLL_28_ScreenFade->vtbl->draw(gdl, &gCurMtx, &gCurVtx);
    gDLL_22_Subtitles->vtbl->func_578(gdl);
    camera_tick();
    func_800129E4();
    // @recomp: Trigger framebuffer FX from debug UI window. This call must be deferred because
    //          the debug UI runs before the framebuffer FX handler, which would cause it to be
    //          processed a frame early (and doesn't work with the recomp patches for it). Calling
    //          it here makes the debug UI menu behave the same as normal game code using framebuffer FX.
    if (recomp_fbfxShouldPlay) {
        recomp_fbfxShouldPlay = FALSE;
        NOTosSetTime(recomp_fbfxTargetID, recomp_fbfxTargetDuration);
    }
    // @recomp: Hook game_tick, before we end the frame
    recomp_game_tick_hook();
    diPrintfAll(gdl);
    // @recomp: Recomp framebuffer FX preparations
    recomp_fbfx_prepare();
    // @recomp: Motion blur framebuffer FX
    recomp_fbfx_motion_blur_tick();
    
    // @recomp: Draw invisible fullscreen rect to avoid RT64 issue where it sometimes thinks
    //          the final framebuffer isn't fullscreen and doesn't apply aspect ratio correction
    //          in fullscreen. The result is all 2D rendering getting stretched out horizontally
    //          when it shouldn't. The game creates a weird situation for RT64 since the shadow
    //          rendering causes a framebuffer flush in the middle of rendering on RT64's side,
    //          so RT64 doesn't factor in the dimensions of the drawing from earlier in the frame,
    //          which does contain fullscreen draws. This also avoids an issue with some transparent
    //          renders like water getting cutoff at certain camera angles.
    u32 viSize = vi_get_current_size();
    u32 viWidth = GET_VIDEO_WIDTH(viSize);
    u32 viHeight = GET_VIDEO_HEIGHT(viSize);

    gDPSetCombineMode((*gdl)++, G_CC_SHADE, G_CC_SHADE); 
    gDPSetOtherMode((*gdl)++, 
        G_AD_PATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | 
            G_TP_PERSP | G_CYC_1CYCLE |  G_PM_NPRIMITIVE, 
        G_AC_NONE | G_ZS_PIXEL | G_RM_XLU_SURF | G_RM_XLU_SURF2);
    gDPSetScissor((*gdl)++, G_SC_NON_INTERLACE, 0, 0, viWidth, viHeight);
    gDPSetFillColor((*gdl)++, (GPACK_RGBA5551(0, 0, 0, 0) << 16) | GPACK_RGBA5551(0, 0, 0, 0));
    gDPFillRectangle((*gdl)++, 0, 0, viWidth, viHeight);

    // @recomp: Take pause screenshot with the DP so RT64 can display the high res version instead
    if (get_pause_state() == 1) {
        recomp_take_pause_screenshot(gdl);
    }

    gDPFullSync(gCurGfx++);
    gSPEndDisplayList(gCurGfx++);

    gfxtask_wait();
    obj_do_deferred_free();
    mmFreeTick();

    if (gPauseState == 0) {
        func_80001A3C();
    }

    gUpdateRate = vi_frame_sync(0);
    
    if (0) {}

    updateRate = (u32) gUpdateRate;
    if (gUpdateRate > 6) {
        gUpdateRate = 6;
        updateRate = gUpdateRate;
    }
    gUpdateRateF = (f32) updateRate;
    gUpdateRateInverseF = 1.0f / gUpdateRateF;
    gUpdateRateMirror = updateRate;
    gUpdateRateMirrorF = gUpdateRateF;
    gUpdateRateInverseMirrorF = 1.0f / gUpdateRateMirrorF;

    func_80014074();
    write_c_file_label_pointers("main/main.c", 0x37C);
}
