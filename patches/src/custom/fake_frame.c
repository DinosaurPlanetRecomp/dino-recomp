#include "patches.h"
#include "patches/main.h"
#include "rt64_extended_gbi.h"
#include "ui_funcs.h"
#include "recomp_options.h"

#include "PR/ultratypes.h"
#include "PR/gbi.h"
#include "PR/os.h"
#include "sys/gfx/gx.h"
#include "sys/gfx/map.h"
#include "sys/main.h"
#include "sys/rcp.h"
#include "sys/rsp_segment.h"
#include "functions.h"
#include "types.h"

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

extern void vi_swap_buffers(void);
extern void vi_func_8005DEE8(void);

static s32 recomp_fake_frame_vi_sync(void) {
    s32 updateRate;

    updateRate = 1;

    vi_swap_buffers();

    while (osRecvMesg(&gVideoMesgQueue, NULL, OS_MESG_NOBLOCK) != -1) {
        updateRate += 1;
    }

    gViUpdateRate = updateRate;

    if (gViUpdateRate < gViUpdateRateTarget) {
        gViUpdateRate = gViUpdateRateTarget;
    }

    while (updateRate < gViUpdateRate) {
        osRecvMesg(&gVideoMesgQueue, NULL, OS_MESG_BLOCK);
        updateRate++;
    }

    osViSwapBuffer(gFramebufferCurrent);

    osRecvMesg(&gVideoMesgQueue, NULL, OS_MESG_BLOCK);

    return updateRate;
}

static void recomp_fake_frame_rdp_init(Gfx **gdl) {
    s32 resolution;
    s32 resWidth, resHeight;
    s32 ulx, uly, lrx, lry;

    func_80002130(&ulx, &uly, &lrx, &lry);

    resolution = vi_get_current_size();
    resWidth = GET_VIDEO_WIDTH(resolution);
    resHeight = GET_VIDEO_HEIGHT(resolution);

    gDPSetScissor((*gdl)++, G_SC_NON_INTERLACE, 0, 0, resWidth - 1, resHeight - 1);

    gDPSetCombineMode((*gdl), G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    dl_apply_combine(gdl);

    gDPSetOtherMode((*gdl), 
        G_AD_PATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_FILL |  G_PM_NPRIMITIVE, 
        G_AC_NONE | G_ZS_PIXEL | G_RM_OPA_SURF | G_RM_OPA_SURF2);
    dl_apply_other_mode(gdl);

    if (gDLBuilder->needsPipeSync) {
        gDLBuilder->needsPipeSync = FALSE;
        gDPPipeSync((*gdl)++);
    }

    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, resWidth, SEGMENT_ADDR(SEGMENT_ZBUFFER, 0x0));

    if (gDLBuilder->needsPipeSync) {
        gDLBuilder->needsPipeSync = FALSE;
        gDPPipeSync((*gdl)++);
    }

    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, resWidth, SEGMENT_ADDR(SEGMENT_FRAMEBUFFER, 0x0));

    func_80002490(gdl);
}

void recomp_do_fake_frame_start(void) {
    u8 phi_v1;
    u32 updateRate;

    osSetTime(0);
    // dl_next_debug_info_set();

    // unused return type
    gfxtask_run_xbus(gMainGfx[gFrameBufIdx], gCurGfx, 0);

    gFrameBufIdx ^= 1;
    gCurGfx = gMainGfx[gFrameBufIdx];
    gCurMtx = gMainMtx[gFrameBufIdx];
    gCurVtx = gMainVtx[gFrameBufIdx];
    gCurPol = gMainPol[gFrameBufIdx];

    // @recomp: Enable RT64 extended GBI commands
    gEXEnable(gCurGfx++);

    // @recomp: Do partial recomp game tick callback (no game logic)
    recomp_run_ui_callbacks();
    recomp_pull_game_options();
    recomp_dbgui_tick();

    // dl_add_debug_info(gCurGfx, 0, "main/main.c", 0x28E);
    rsp_segment(&gCurGfx, SEGMENT_MAIN, (void *)K0BASE);
    rsp_segment(&gCurGfx, SEGMENT_FRAMEBUFFER, gFramebufferCurrent);
    rsp_segment(&gCurGfx, SEGMENT_ZBUFFER, D_800BCCB4);
    //func_8003E9F0(&gCurGfx, gUpdateRate);
    dl_set_all_dirty();
    func_8003DB5C();

    if (gDLBuilder->needsPipeSync != 0) {
        gDLBuilder->needsPipeSync = 0;
        gDPPipeSync(gCurGfx++);
    }

    gDPSetDepthImage(gCurGfx++, SEGMENT_ZBUFFER << 24);

    rsp_init(&gCurGfx);
    // phi_v1 = 2;

    // if (func_80041D5C() == 0)
    //     phi_v1 = 0;
    // else if (func_80041D74() == 0)
    //     phi_v1 = 3;

    // func_80037A14(&gCurGfx, &gCurMtx, phi_v1);
    recomp_fake_frame_rdp_init(&gCurGfx);
}

void recomp_do_fake_frame_end(void) {
    u32 updateRate;

    // rt64 hack
    u32 viSize = vi_get_current_size();
    u32 viWidth = GET_VIDEO_WIDTH(viSize);
    u32 viHeight = GET_VIDEO_HEIGHT(viSize);

    gDPSetCombineMode(gCurGfx++, G_CC_SHADE, G_CC_SHADE); 
    gDPSetOtherMode(gCurGfx++, 
        G_AD_PATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | 
            G_TP_PERSP | G_CYC_1CYCLE |  G_PM_NPRIMITIVE, 
        G_AC_NONE | G_ZS_PIXEL | G_RM_XLU_SURF | G_RM_XLU_SURF2);
    gDPSetScissor(gCurGfx++, G_SC_NON_INTERLACE, 0, 0, viWidth, viHeight);
    gDPSetFillColor(gCurGfx++, (GPACK_RGBA5551(0, 0, 0, 0) << 16) | GPACK_RGBA5551(0, 0, 0, 0));
    gDPFillRectangle(gCurGfx++, 0, 0, viWidth, viHeight);
    // rt64 hack

    gDPFullSync(gCurGfx++);
    gSPEndDisplayList(gCurGfx++);

    gfxtask_wait();
    // obj_do_deferred_free();
    // mmFreeTick();

    if (gPauseState == 0) {
        func_80001A3C();
    }

    //gUpdateRate = vi_frame_sync(0);
    gUpdateRate = recomp_fake_frame_vi_sync();
    
    //if (0) {}

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
    
    // func_80014074();
    // write_c_file_label_pointers("main/main.c", 0x37C);
}
