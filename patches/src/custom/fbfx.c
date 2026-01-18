#include "patches.h"
#include "recomp_funcs.h"
#include "patches/fake_frame.h"

#include "PR/ultratypes.h"
#include "PR/gbi.h"
#include "PR/mbi.h"
#include "sys/gfx/gx.h"
#include "sys/main.h"
#include "sys/rsp_segment.h"

extern Gfx *gCurGfx;

extern s32 D_80092A50;
extern s32 D_80092A54;
extern s32 D_800B49E0;

u16 recomp_fbfxNextFramebufferSnapshot[320 * 260];

static s32 recomp_fps_aware_accum_alpha(s32 alpha, f32 targetFramerate) {
    // https://github.com/Zelda64Recomp/Zelda64Recomp/blob/ab677e76615e5e47b3b26c822ca426485752ac77/patches/effect_patches.c#L90-L102
    f32 exponent = targetFramerate / (f32)recomp_get_refresh_rate();
    f32 alpha_float = recomp_powf(alpha / 255.0f, exponent);
    // Cap for higher framerates
    // TODO: why doesn't high precision framebuffer avoid this issue?
    alpha_float = MIN(alpha_float, 0.825f);
    return (s32)(alpha_float * 255.0f);
}

static void recomp_fbfx_snapshot_framebuffer(Gfx **gdl, u16 *src, u16 *dst) {
    u32 viSize = vi_get_current_size();
    u32 viWidth = GET_VIDEO_WIDTH(viSize);
    u32 viHeight = GET_VIDEO_HEIGHT(viSize);

    // Use DP to copy framebuffer so RT64 uses the high res version when we go to draw it later
    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, dst);
    gDPLoadTextureTile((*gdl)++, src, G_IM_FMT_RGBA, G_IM_SIZ_16b, 
        viWidth, viHeight, 
        0, 0, viWidth, viHeight, 
        0, 
        G_TX_NOMIRROR | G_TX_WRAP | G_TX_NOMASK, 
        G_TX_NOMIRROR | G_TX_WRAP | G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOMASK,
        G_TX_NOLOD, G_TX_NOLOD);
    
    gSPClearGeometryMode((*gdl)++, 0xFFFFFF);
    gDPSetCombineMode((*gdl)++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPSetOtherMode((*gdl)++, 
        G_AD_PATTERN | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | 
            G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_COPY | G_PM_NPRIMITIVE, 
        G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_NOOP2);
    
    gSPTextureRectangle((*gdl)++, 0, 0, (320 + 1) * 4, (240 + 1) * 4, 0, 0, 0, 1 << 12, 1 << 10);

    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, viWidth, SEGMENT_ADDR(SEGMENT_FRAMEBUFFER, 0x0));
}

static void recomp_fbfx_3(void) {
    s32 iterationCount = MIN(D_80092A54, 15);
    
    // Console timing for this effect: ~0.041769467 seconds per iteration
    const f32 secondsPerIteration = 0.041769467f;
    const f32 targetFramerate = 1.0f / secondsPerIteration;
    f32 durationSeconds = iterationCount * secondsPerIteration;
    f32 timeLeft = durationSeconds;

    while (timeLeft > 0.0f) {
        recomp_do_fake_frame_start();

        Gfx **gdl = &gCurGfx;

        u32 viSize = vi_get_current_size();
        u32 viWidth = GET_VIDEO_WIDTH(viSize);
        u32 viHeight = GET_VIDEO_HEIGHT(viSize);

        // Calculate alpha the same way as vanilla, but with floating point accuracy so we can run at higher framerates
        f32 i = (durationSeconds - timeLeft) / secondsPerIteration;
        f32 var_v1_2 = (iterationCount - i) / 3.0f;
        if (var_v1_2 >= 5.0f) {
            var_v1_2 = -1.0f;
        }
        f32 alphaF = var_v1_2 == -1.0f ? 0.0f : 1.0f / recomp_powf(2, var_v1_2);
        s32 alpha = (s32)(alphaF * 255.0f);
        // Effect is cummulative, so scale with framerate
        alpha = 255 - recomp_fps_aware_accum_alpha(255 - alpha, targetFramerate);

        // Setup
        gDPPipeSync((*gdl)++);
        gSPClearGeometryMode((*gdl)++, 0xFFFFFF);
        gDPSetScissor((*gdl)++, G_SC_NON_INTERLACE, 0, 0, viWidth, viHeight);

        gDPSetCombineMode((*gdl)++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);    
        gDPSetOtherMode((*gdl)++, 
            G_AD_PATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | 
            G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | 
            G_PM_NPRIMITIVE, G_AC_NONE | G_ZS_PIXEL | G_RM_XLU_SURF | G_RM_XLU_SURF2);
        
        // Redraw current frame as base 
        gDPSetPrimColor((*gdl)++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        gDPLoadTextureTile((*gdl)++, gFramebufferNext, G_IM_FMT_RGBA, G_IM_SIZ_16b,
            viWidth, viHeight,
            0, 0,
            viWidth - 1, viHeight - 1, 0, 
            G_TX_CLAMP | G_TX_NOMIRROR, 
            G_TX_CLAMP | G_TX_NOMIRROR,
            G_TX_NOMASK, G_TX_NOMASK,
            G_TX_NOLOD, G_TX_NOLOD
        );
        gSPTextureRectangle((*gdl)++, 0, 0, (viWidth) * 4, (viHeight) * 4, 0, 0, 0, 1 << 10, 1 << 10);
        
        // Add next frame on top with transparency
        gDPSetPrimColor((*gdl)++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
        gDPLoadTextureTile((*gdl)++, recomp_fbfxNextFramebufferSnapshot, G_IM_FMT_RGBA, G_IM_SIZ_16b,
            viWidth, viHeight,
            0, 0,
            viWidth - 1, viHeight - 1, 0, 
            G_TX_CLAMP | G_TX_NOMIRROR, 
            G_TX_CLAMP | G_TX_NOMIRROR,
            G_TX_NOMASK, G_TX_NOMASK,
            G_TX_NOLOD, G_TX_NOLOD
        );
        gSPTextureRectangle((*gdl)++, 0, 0, (viWidth) * 4, (viHeight) * 4, 0, 0, 0, 1 << 10, 1 << 10);
        
        recomp_do_fake_frame_end();

        timeLeft -= (1.0f/60.0f) * gUpdateRateF;
    }
}

void recomp_fbfx(void) {
    if (D_80092A50 == 0) {
        return;
    }

    switch (D_80092A50) {
        case 3:
            recomp_fbfx_3();
            break;
        case 10:
            // Handled separately since this effect runs with normal gameplay.
            break;
        default:
            recomp_eprintf("[recomp fbfx] Triggered framebuffer FX is unimplemented! ID: %d Duration: %d\n", 
                D_80092A50, D_80092A54);
            break;
    }
}

void recomp_fbfx_prepare(void) {
    if (D_80092A50 == 0 || D_80092A50 == 10) {
        return;
    }

    Gfx **gdl = &gCurGfx;

    u32 viSize = vi_get_current_size();
    u32 viWidth = GET_VIDEO_WIDTH(viSize);
    u32 viHeight = GET_VIDEO_HEIGHT(viSize);

    gDPSetScissor((*gdl)++, G_SC_NON_INTERLACE, 0, 0, viWidth, viHeight);

    // Copy next frame
    recomp_fbfx_snapshot_framebuffer(gdl, gFramebufferCurrent, recomp_fbfxNextFramebufferSnapshot);
    
    // Keep displaying the previous frame
    gDPSetCombineMode((*gdl)++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPSetOtherMode((*gdl)++, 
        G_AD_PATTERN | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | 
            G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_COPY | G_PM_NPRIMITIVE, 
        G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_NOOP2);
    gDPLoadTextureTile((*gdl)++, gFramebufferNext, G_IM_FMT_RGBA, G_IM_SIZ_16b, 
            viWidth, viHeight, 
            0, 0, viWidth, viHeight, 
            0, 
            G_TX_NOMIRROR | G_TX_WRAP | G_TX_NOMASK, 
            G_TX_NOMIRROR | G_TX_WRAP | G_TX_NOMASK,
            G_TX_NOMASK, G_TX_NOMASK,
            G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle((*gdl)++, 0, 0, (viWidth + 1) * 4, (viHeight + 1) * 4, 0, 0, 0, 1 << 12, 1 << 10);
}

void recomp_fbfx_motion_blur_tick(void) {
    if (D_80092A50 != 10) {
        return;
    }

    Gfx **gdl = &gCurGfx;

    u32 viSize = vi_get_current_size();
    u32 viWidth = GET_VIDEO_WIDTH(viSize);
    u32 viHeight = GET_VIDEO_HEIGHT(viSize);

    // scale effect with framerate
    s32 alpha = recomp_fps_aware_accum_alpha(128, 30.0f);

    gDPSetScissor((*gdl)++, G_SC_NON_INTERLACE, 0, 0, viWidth, viHeight);
    gSPClearGeometryMode((*gdl)++, 0xFFFFFF);
    gDPSetCombineMode((*gdl)++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);    
    gDPSetOtherMode((*gdl)++, 
        G_AD_PATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | 
        G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | 
        G_PM_NPRIMITIVE, G_AC_NONE | G_ZS_PIXEL | G_RM_XLU_SURF | G_RM_XLU_SURF2);
    gDPSetPrimColor((*gdl)++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
    gDPLoadTextureTile((*gdl)++, gFramebufferNext, G_IM_FMT_RGBA, G_IM_SIZ_16b,
        viWidth, viHeight,
        0, 0,
        viWidth - 1, viHeight - 1, 0, 
        G_TX_CLAMP | G_TX_NOMIRROR, 
        G_TX_CLAMP | G_TX_NOMIRROR,
        G_TX_NOMASK, G_TX_NOMASK,
        G_TX_NOLOD, G_TX_NOLOD
    );
    gSPTextureRectangle((*gdl)++, 0, 0, (viWidth) * 4, (viHeight) * 4, 0, 0, 0, 1 << 10, 1 << 10);
}
