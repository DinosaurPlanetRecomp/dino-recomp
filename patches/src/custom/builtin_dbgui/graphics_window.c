#include "dbgui.h"
#include "patches.h"
#include "patches/main.h"
#include "patches/vi.h"
#include "recomp_funcs.h"

#include "sys/camera.h"
#include "sys/gfx/gx.h"
#include "sys/main.h"
#include "sys/math.h"
#include "types.h"
#include "variables.h"

extern f32 gFovY;
extern f32 gAspect;
extern u16 gPerspNorm;
extern MtxF gViewProjMtx;
extern MtxF gProjectionMtx;
extern MtxF gViewMtx;
extern f32 gNearPlane;
extern f32 gFarPlane;
extern Viewport gViewports[4];
extern Vp gRSPViewports[20];

extern Gfx *gMainGfx[2];
extern Gfx *gCurGfx;
extern Mtx *gMainMtx[2];
extern Mtx *gCurMtx;
extern Vertex *gMainVtx[2];
extern Vertex *gCurVtx;
extern Triangle *gMainPol[2];
extern Triangle *gCurPol;
extern u8 gFrameBufIdx;

extern s16 gRenderListLength;

extern s32 D_80092A50;
extern s32 D_80092A54;
extern s32 D_800B49E0;

s32 recomp_fbfxShouldPlay = FALSE;
s32 recomp_fbfxTargetID = 3;
s32 recomp_fbfxTargetDuration = 15;

static s32 gCurGfx_overflowed = FALSE;
static s32 gCurMtx_overflowed = FALSE;
static s32 gCurVtx_overflowed = FALSE;
static s32 gCurPol_overflowed = FALSE;

static u32 renderListLength;
static u32 gdlSize;
static u32 mtxSize;
static u32 vtxSize;
static u32 polSize;

void graphics_window_check_buffer_sizes(void) {
    renderListLength = gRenderListLength;

    gdlSize = (u32)gCurGfx - (u32)gMainGfx[gFrameBufIdx];
    mtxSize = (u32)gCurMtx - (u32)gMainMtx[gFrameBufIdx];
    vtxSize = (u32)gCurVtx - (u32)gMainVtx[gFrameBufIdx];
    polSize = (u32)gCurPol - (u32)gMainPol[gFrameBufIdx];

    if (gdlSize > RECOMP_MAIN_GFX_BUF_SIZE) gCurGfx_overflowed = TRUE;
    if (mtxSize > RECOMP_MAIN_MTX_BUF_SIZE) gCurMtx_overflowed = TRUE;
    if (vtxSize > RECOMP_MAIN_VTX_BUF_SIZE) gCurVtx_overflowed = TRUE;
    if (polSize > RECOMP_MAIN_POL_BUF_SIZE) gCurPol_overflowed = TRUE;

    if (gCurGfx_overflowed) recomp_eprintf("WARN: gCurGfx overflowed!!!\n");
    if (gCurMtx_overflowed) recomp_eprintf("WARN: gCurMtx overflowed!!!\n");
    if (gCurVtx_overflowed) recomp_eprintf("WARN: gCurVtx overflowed!!!\n");
    if (gCurPol_overflowed) recomp_eprintf("WARN: gCurPol overflowed!!!\n");
}

static void general_tab(void) {
    dbgui_textf("gRenderListLength: %d", renderListLength);
    dbgui_textf("gWorldX: %f", gWorldX);
    dbgui_textf("gWorldZ: %f", gWorldZ);

    dbgui_textf("gCurGfx size: %x/%x\t(%f%%)", gdlSize, RECOMP_MAIN_GFX_BUF_SIZE, (f32)gdlSize / (f32)RECOMP_MAIN_GFX_BUF_SIZE);
    dbgui_textf("gCurMtx size: %x/%x\t(%f%%)", mtxSize, RECOMP_MAIN_MTX_BUF_SIZE, (f32)mtxSize / (f32)RECOMP_MAIN_MTX_BUF_SIZE);
    dbgui_textf("gCurVtx size: %x/%x\t(%f%%)", vtxSize, RECOMP_MAIN_VTX_BUF_SIZE, (f32)vtxSize / (f32)RECOMP_MAIN_VTX_BUF_SIZE);
    dbgui_textf("gCurPol size: %x/%x\t(%f%%)", polSize, RECOMP_MAIN_POL_BUF_SIZE, (f32)polSize / (f32)RECOMP_MAIN_POL_BUF_SIZE);
}

static void camera_tab(void) {
    dbgui_textf("gFovY: %f", gFovY);
    dbgui_textf("gAspect: %f", gAspect);
    dbgui_textf("gNearPlane: %f", gNearPlane);
    dbgui_textf("gFarPlane: %f", gFarPlane);
    dbgui_textf("gPerspNorm: %d", gPerspNorm);

    if (dbgui_tree_node("gProjectionMtx")) {
        for (int i = 0; i < 4; i++) {
            dbgui_textf("%f %f %f %f", gProjectionMtx.m[i][0], gProjectionMtx.m[i][1], gProjectionMtx.m[i][2], gProjectionMtx.m[i][3]);
        }

        dbgui_tree_pop();
    }

    if (dbgui_tree_node("gViewMtx")) {
        for (int i = 0; i < 4; i++) {
            dbgui_textf("%f %f %f %f", gViewMtx.m[i][0], gViewMtx.m[i][1], gViewMtx.m[i][2], gViewMtx.m[i][3]);
        }

        dbgui_tree_pop();
    }

    if (dbgui_tree_node("gViewProjMtx")) {
        for (int i = 0; i < 4; i++) {
            dbgui_textf("%f %f %f %f", gViewProjMtx.m[i][0], gViewProjMtx.m[i][1], gViewProjMtx.m[i][2], gViewProjMtx.m[i][3]);
        }

        dbgui_tree_pop();
    }

    if (dbgui_tree_node("gViewports")) {
        for (s32 i = 0; i < 4; i++) {
            Viewport *viewport = &gViewports[i];

            if (dbgui_tree_node(recomp_sprintf_helper("[%d]", i))) {
                dbgui_textf("x1: %d", viewport->x1);
                dbgui_textf("y1: %d", viewport->y1);
                dbgui_textf("x2: %d", viewport->x2);
                dbgui_textf("y2: %d", viewport->y2);
                dbgui_textf("posX: %d", viewport->posX);
                dbgui_textf("posY: %d", viewport->posY);
                dbgui_textf("width: %d", viewport->width);
                dbgui_textf("height: %d", viewport->height);
                dbgui_textf("ulx: %d", viewport->ulx);
                dbgui_textf("uly: %d", viewport->uly);
                dbgui_textf("lrx: %d", viewport->lrx);
                dbgui_textf("lry: %d", viewport->lry);
                dbgui_textf("flags: %d", viewport->flags);

                dbgui_tree_pop();
            }
        }

        dbgui_tree_pop();
    }

    if (dbgui_tree_node("gRSPViewports")) {
        for (s32 i = 0; i < 28; i++) {
            Vp *viewport = &gRSPViewports[i];

            if (dbgui_tree_node(recomp_sprintf_helper("[%d]", i))) {
                dbgui_textf("vscale[0]: %d", viewport->vp.vscale[0]);
                dbgui_textf("vscale[1]: %d", viewport->vp.vscale[1]);
                dbgui_textf("vscale[2]: %d", viewport->vp.vscale[2]);
                dbgui_textf("vscale[3]: %d", viewport->vp.vscale[3]);
                dbgui_textf("vtrans[0]: %d", viewport->vp.vtrans[0]);
                dbgui_textf("vtrans[1]: %d", viewport->vp.vtrans[1]);
                dbgui_textf("vtrans[2]: %d", viewport->vp.vtrans[2]);
                dbgui_textf("vtrans[3]: %d", viewport->vp.vtrans[3]);

                dbgui_tree_pop();
            }
        }

        dbgui_tree_pop();
    }
}

static void video_tab(void) {
    u32 res = vi_get_current_size();
    u32 width = GET_VIDEO_WIDTH(res);
    u32 height = GET_VIDEO_HEIGHT(res);

    dbgui_textf("VI size: %dx%d", width, height);
    dbgui_textf("gDisplayHertz: %d", gDisplayHertz);
    dbgui_textf("gUpdateRate: %d", gUpdateRate);
    dbgui_textf("gUpdateRateF: %f", gUpdateRateF);
    dbgui_textf("gViUpdateRateTarget: %d", gViUpdateRateTarget);
    dbgui_textf("gViBlackTimer: %d", gViBlackTimer);
    dbgui_textf("gAspectRatio: %f", gAspectRatio);
    dbgui_textf("gViHeightRatio: %f", gViHeightRatio);
    dbgui_textf("gHStartMod: %d", gHStartMod);
    dbgui_textf("gVScaleMod: %d", gVScaleMod);
    dbgui_textf("osTvType: %d", osTvType);
    dbgui_textf("gVideoMode: %d", gVideoMode);
    if (dbgui_tree_node("gTvViMode")) {
        dbgui_textf("type: %d", gTvViMode.type);
        dbgui_textf("comRegs.ctrl: %d", gTvViMode.comRegs.ctrl);
        dbgui_textf("comRegs.width: %d", gTvViMode.comRegs.width);
        dbgui_textf("comRegs.burst: %d", gTvViMode.comRegs.burst);
        dbgui_textf("comRegs.vSync: %d", gTvViMode.comRegs.vSync);
        dbgui_textf("comRegs.hSync: %d", gTvViMode.comRegs.hSync);
        dbgui_textf("comRegs.leap: %d", gTvViMode.comRegs.leap);
        dbgui_textf("comRegs.hStart: %d", gTvViMode.comRegs.hStart);
        dbgui_textf("comRegs.xScale: %d", gTvViMode.comRegs.xScale);
        dbgui_textf("comRegs.vCurrent: %d", gTvViMode.comRegs.vCurrent);
        dbgui_textf("fldRegs[0].origin: %d", gTvViMode.fldRegs[0].origin);
        dbgui_textf("fldRegs[0].yScale: %d", gTvViMode.fldRegs[0].yScale);
        dbgui_textf("fldRegs[0].vStart: %d", gTvViMode.fldRegs[0].vStart);
        dbgui_textf("fldRegs[0].vBurst: %d", gTvViMode.fldRegs[0].vBurst);
        dbgui_textf("fldRegs[0].vIntr: %d", gTvViMode.fldRegs[0].vIntr);
        dbgui_textf("fldRegs[1].origin: %d", gTvViMode.fldRegs[1].origin);
        dbgui_textf("fldRegs[1].yScale: %d", gTvViMode.fldRegs[1].yScale);
        dbgui_textf("fldRegs[1].vStart: %d", gTvViMode.fldRegs[1].vStart);
        dbgui_textf("fldRegs[1].vBurst: %d", gTvViMode.fldRegs[1].vBurst);
        dbgui_textf("fldRegs[1].vIntr: %d", gTvViMode.fldRegs[1].vIntr);
        
        dbgui_tree_pop();
    }
}

static void fbfx_tab(void) {
    static s32 continuous = FALSE;
    if (dbgui_input_int("FX ID", &recomp_fbfxTargetID)) {
        if (recomp_fbfxTargetID < 0) recomp_fbfxTargetID = 0;
        if (recomp_fbfxTargetID > 15) recomp_fbfxTargetID = 15;
    }
    if (dbgui_input_int("FX Duration", &recomp_fbfxTargetDuration)) {
        if (recomp_fbfxTargetDuration < 0) recomp_fbfxTargetDuration = 0;
    }
    dbgui_checkbox("Autoplay", &continuous);
    if (continuous) {
        recomp_fbfxShouldPlay = TRUE;
    }
    if (!continuous) {
        dbgui_same_line();
        if (dbgui_button("Play")) {
            recomp_fbfxShouldPlay = TRUE;
        }
    }
    dbgui_separator();
    dbgui_textf("Current FX:");
    dbgui_textf("  ID: %d", D_80092A50);
    dbgui_textf("  Duration: %d", D_80092A54);
    dbgui_textf("  Timer: %d", D_800B49E0);

    dbgui_separator();

    static s32 cycles = 0;
    static s32 microseconds = 0;
    if (dbgui_input_int("Clock Cycles", &cycles)) {
        microseconds = (cycles * 64) / 3000; // OS_CYCLES_TO_USEC
    }

    dbgui_textf("= %d microseconds", microseconds);
    dbgui_textf("= %f seconds", ((f32)microseconds / 1000000.0f));
}

static void hacks_tab(void) {
    dbgui_checkbox("30 FPS SnowBike race", &recomp_snowbike30FPS);
}

static void recomp_tab(void) {
    u32 width, height;
    recomp_get_window_resolution(&width, &height);

    dbgui_textf("Recomp window resolution: %ux%u", width, height);

    const char *recompAspectStr = "Unknown";
    u32 recompAspect = recomp_get_aspect_ratio_mode();
    switch (recompAspect) {
        case RECOMP_ASPECT_ORIGINAL:
            recompAspectStr = "Original";
            break;
        case RECOMP_ASPECT_EXPAND:
            recompAspectStr = "Expand";
            break;
        case RECOMP_ASPECT_MANUAL:
            recompAspectStr = "Manual";
            break;
    }
    dbgui_textf("Recomp aspect ratio mode: %s (%u)", recompAspectStr, recompAspect);
    dbgui_textf("Recomp aspect ratio: %f", recomp_get_aspect_ratio());

    const char *recompHUDStr = "Unknown";
    u32 recompHUD = recomp_get_hud_ratio_mode();
    switch (recompHUD) {
        case RECOMP_HUD_ORIGINAL:
            recompHUDStr = "Original";
            break;
        case RECOMP_HUD_CLAMP16X9:
            recompHUDStr = "Clamp16x9";
            break;
        case RECOMP_HUD_FULL:
            recompHUDStr = "Full";
            break;
    }
    dbgui_textf("Recomp HUD ratio mode: %s (%u)", recompHUDStr, recompHUD);

    dbgui_textf("Recomp refresh rate: %d", recomp_get_refresh_rate());
}

void dbgui_graphics_window(s32 *open) {
    if (dbgui_begin("Graphics Debug", open)) {
        if (dbgui_begin_tab_bar("tabs")) {
            if (dbgui_begin_tab_item("General", NULL)) {
                general_tab();
                dbgui_end_tab_item();
            }

            if (dbgui_begin_tab_item("Camera", NULL)) {
                camera_tab();
                dbgui_end_tab_item();
            }

            if (dbgui_begin_tab_item("Video", NULL)) {
                video_tab();
                dbgui_end_tab_item();
            }

            if (dbgui_begin_tab_item("Framebuffer FX", NULL)) {
                fbfx_tab();
                dbgui_end_tab_item();
            }

            if (dbgui_begin_tab_item("Hacks", NULL)) {
                hacks_tab();
                dbgui_end_tab_item();
            }

            if (dbgui_begin_tab_item("Recomp", NULL)) {
                recomp_tab();
                dbgui_end_tab_item();
            }
            
            dbgui_end_tab_bar();
        }
    }
    dbgui_end();
}
