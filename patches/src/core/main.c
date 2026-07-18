#include "builtin_dbgui.h"
#include "patches.h"
#include "patches/builtin_dbgui/graphics_window.h"
#include "patches/fbfx.h"
#include "patches/main.h"
#include "patches/rcp.h"
#include "dbgui.h"
#include "recomp_options.h"
#include "recomp_funcs.h"
#include "matrix_groups.h"
#include "ui_funcs.h"

#include "libnaudio/n_unkfuncs.h"
#include "game/gamebits.h"
#include "sys/audio.h"
#include "sys/asset.h"
#include "sys/di_rcp.h"
#include "sys/exception.h"
#include "sys/rcp.h"
#include "sys/rsp_segment.h"
#include "sys/main.h"
#include "sys/memory.h"
#include "sys/objects.h"
#include "sys/print.h"
#include "sys/framebuffer_fx.h"
#include "sys/map.h"
#include "sys/menu.h"
#include "sys/vi.h"
#include "sys/joypad.h"
#include "types.h"
#include "dll.h"

RECOMP_DECLARE_EVENT(recomp_on_game_tick_start());
RECOMP_DECLARE_EVENT(recomp_on_game_tick());
RECOMP_DECLARE_EVENT(recomp_on_dbgui());

_Bool recomp_frameInterpActive;
_Bool recomp_skipAllInterp;

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
extern s8 D_8008C94C;

extern u8 D_8008CA30;
extern s32 gMainMapChangeNextMenu;
extern s8 gMainDoMapChange;

extern void main_func_80013D80(void);
extern void mainHandleMapChange(void);
extern s8 main_func_800143FC(void);
extern void mainUpdatePlayerPosBuffer(void);

u32 recomp_tickCounter;

// @recomp: Move graphics buffers into patch memory to save vanilla pool memory
static Gfx recompMainGfx[2][RECOMP_MAIN_GFX_BUF_SIZE / sizeof(Gfx)]; 
static Mtx recompMainMtx[2][RECOMP_MAIN_MTX_BUF_SIZE / sizeof(Mtx)]; 
static Vertex recompMainVtx[2][RECOMP_MAIN_VTX_BUF_SIZE / sizeof(Vertex)]; 
static Triangle recompMainPol[2][RECOMP_MAIN_POL_BUF_SIZE / sizeof(Triangle)]; 

RECOMP_PATCH void mainAllocFrameBuffers(void) {
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
            dbgui_text("| Press ` or F9 to close the debug UI.");
            dbgui_end_main_menu_bar();
        }
    }

    dbgui_ui_frame_end();
}

static void recomp_game_tick_start_hook(void) {
    recomp_on_game_tick_start();
    recomp_run_ui_callbacks();
    recomp_frameInterpActive = recomp_is_frame_interp_active();
    recomp_pull_game_options();
    recomp_dbgui_tick();
}

static void recomp_game_tick_hook(void) {
    recomp_on_game_tick();
    builtin_dbgui_game_tick();
}

RECOMP_PATCH void mainTick(void) {
    u8 clearFlags;
    u32 updateRate;
    Gfx **gdl;

    // @recomp: Do custom framebuffer FX implementation
    recomp_fbfx();

    osSetTime(0);
    diRcpTraceReset();

    gdl = &gCurGfx;

    // unused return type
    rcpF3DEX_2_XBUS(gMainGfx[gFrameBufIdx], gCurGfx, 0);

    gFrameBufIdx ^= 1;
    gCurGfx = gMainGfx[gFrameBufIdx];
    gCurMtx = gMainMtx[gFrameBufIdx];
    gCurVtx = gMainVtx[gFrameBufIdx];
    gCurPol = gMainPol[gFrameBufIdx];

    // @recomp: Enable RT64 extended GBI commands
    gEXEnable((*gdl)++);

    // @recomp: Hook start of game_tick
    recomp_game_tick_start_hook();

    diRcpTrace(gCurGfx, 0, "main/main.c", 0x28E);
    segSetBase(&gCurGfx, SEGMENT_MAIN, (void *)K0BASE);
    segSetBase(&gCurGfx, SEGMENT_FRAMEBUFFER, gFrontFramebuffer);
    segSetBase(&gCurGfx, SEGMENT_ZBUFFER, gFrontDepthBuffer);
    fbfxTick(&gCurGfx, gUpdateRate);
    dlSetAllDirty();
    texRenderReset();

    if (gDLBuilder->needsPipeSync != 0) {
        gDLBuilder->needsPipeSync = 0;
        gDPPipeSync(gCurGfx++);
    }

    gDPSetDepthImage(gCurGfx++, SEGMENT_ADDR(SEGMENT_ZBUFFER, 0));

    rcpInitSp(&gCurGfx);

    clearFlags = CLEAR_ZBUFFER;
    if (trackIsZBufferOn() == FALSE) {
        clearFlags = CLEAR_NONE;
    } else if (trackIsSkyOn() == FALSE) {
        clearFlags = CLEAR_COLOR | CLEAR_ZBUFFER;
    }

    rcpClearScreen(&gCurGfx, &gCurMtx, clearFlags);
    voxUpdateCacheTimers();
    main_func_80013D80();
    am_func_800121DC();
    gDLL_28_ScreenFade->vtbl->draw(gdl, &gCurMtx, &gCurVtx);
    gDLL_22_Subtitles->vtbl->func_578(gdl);
    camTick();
    assetQueueTick();
    // @recomp: Trigger framebuffer FX from debug UI window. This call must be deferred because
    //          the debug UI runs before the framebuffer FX handler, which would cause it to be
    //          processed a frame early (and doesn't work with the recomp patches for it). Calling
    //          it here makes the debug UI menu behave the same as normal game code using framebuffer FX.
    if (recomp_fbfxShouldPlay) {
        recomp_fbfxShouldPlay = FALSE;
        fbfxPlay(recomp_fbfxTargetID, recomp_fbfxTargetDuration);
    }
    // @recomp: Hook game_tick, before we end the frame
    recomp_game_tick_hook();
    diPrintfAll(gdl);

    // @recomp: Draw invisible fullscreen rect to avoid RT64 issue where it sometimes thinks
    //          the final framebuffer isn't fullscreen and doesn't apply aspect ratio correction
    //          in fullscreen. The result is all 2D rendering getting stretched out horizontally
    //          when it shouldn't. The game creates a weird situation for RT64 since the shadow
    //          rendering causes a framebuffer flush in the middle of rendering on RT64's side,
    //          so RT64 doesn't factor in the dimensions of the drawing from earlier in the frame,
    //          which does contain fullscreen draws. This also avoids an issue with some transparent
    //          renders like water getting cutoff at certain camera angles.
    u32 viSize = viGetCurrentSize();
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

    // @recomp: Motion blur framebuffer FX
    recomp_fbfx_motion_blur_tick();
    // @recomp: Recomp framebuffer FX snapshot
    recomp_fbfx_snapshot();

    // @recomp: Take pause screenshot with the DP so RT64 can display the high res version instead
    if (mainGetPauseState() == 1) {
        recomp_take_pause_screenshot(gdl);
    }

    gDPFullSync(gCurGfx++);
    gSPEndDisplayList(gCurGfx++);

    // @recomp: Re-enable global interp
    recomp_skipAllInterp = FALSE;

    rcpWaitDP();
    objDoDeferredFree();
    mmFreeTick();

    if (gPauseState == 0) {
        camApplyAlternateTrigger();
    }

    gUpdateRate = viFrameSync(0);
    
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

    mainHandleMapChange();
    write_c_file_label_pointers("main/main.c", 0x37C);

    // @recomp: Track semi-unique number for each game tick
    recomp_tickCounter++;
}

RECOMP_PATCH void mainHandleMapChange(void) {
    if (gMainDoMapChange) {
        //recomp_printf("$$$$$  CHANGEMAP \n");
        mmSetDelay(0);
        if (D_8008CA30 != 0) {
            rcpSetScreenColour(0, 0, 0);
            func_800668A4();
            map_func_800484A8();

            gCurGfx = gMainGfx[gFrameBufIdx];
            gDPFullSync(gCurGfx++);
            gSPEndDisplayList(gCurGfx++);
        }

        gMainDoMapChange = FALSE;

        // @recomp: Skip interpolation since everything is being reloaded
        recomp_skip_all_interp();

        mmSetDelay(0);
        camInit();

        if (gMainMapChangeNextMenu >= 0) {
            menuSet(gMainMapChangeNextMenu);
            gMainMapChangeNextMenu = -1;
        }

        map_func_8004773C();

        if (gDLL_23 != NULL) {
            gDLL_23->vtbl->func_18(1);
        }

        mmSetDelay(2);
        D_8008CA30 = 1;
    }
}

RECOMP_PATCH void main_func_80013D80(void) {
    s32 button;

    joyDisableButtons(0, U_JPAD | R_JPAD);
    gDLL_2_Camera->vtbl->lock_icon_tick();
    gDLL_22_Subtitles->vtbl->func_4C0();

    if (menuUpdate1() == 0) {
        button = joyGetPressed(0);

        if (gPauseState != 0) {
            rcpDrawPauseScreenFreezeFrame(&gCurGfx);
        }

        if (gPauseState == 0) {
            objTick();
            trackTick(0);

            if ((camIsAlternateActive() == 0) 
                    && (D_8008C94C == 0) 
                    && (main_func_800143FC() == 0) 
                    && ((button & START_BUTTON) != 0) 
                    && (mainGetBits(BIT_Menus_Selection_Blocked) == 0)) {
                gPauseState = 1;
                joyDisableButtons(0, START_BUTTON);
                // @recomp: Don't switch to pause menu immediately so we include the current menu
                //          in the pause screen snapshot. Note: This is only an issue because of the
                //          changes recomp makes to how the pause screen snapshot is taken.
                //menuSet(MENU_PAUSE);
            }

            gDLL_29_Gplay->vtbl->tick();
        } else {
            objUpdateObjModels();
        }

        if (gPauseState == 0) {
            mainUpdatePlayerPosBuffer();
        }

        menuUpdate2();
        func_800591EC();
        map_func_8004A67C();
        mapUpdateStreaming();
        objHandleAnimseqActors();

        gDLL_4_Race->vtbl->func14();

        // @recomp: Still draw track if we're paused but haven't taken the screen snapshot yet. Otherwise,
        //          the snapshot will be blank in cases where the color framebuffer was cleared at the
        //          start of the frame.
        if (gPauseState != 2) {
            trackDraw(&gCurGfx, &gCurMtx, &gCurVtx, &gCurPol, &gCurVtx, &gCurPol);
        }

        gDLL_20_Screens->vtbl->draw(&gCurGfx);
        menuDraw(&gCurGfx, &gCurMtx, &gCurVtx, &gCurPol);

        // @recomp: Do the switch to the pause menu down here instead
        if (gPauseState == 1) {
            menuSet(MENU_PAUSE);
        }

        D_8008C94C -= gUpdateRate;

        if ((s32)D_8008C94C < 0) {
            D_8008C94C = 0;
        }
    }
}

void recomp_skip_all_interp(void) {
    recomp_skipAllInterp = TRUE;
}
