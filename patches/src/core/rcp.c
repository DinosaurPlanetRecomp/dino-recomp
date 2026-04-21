#include "patches.h"
#include "patches/rcp.h"

#include "PR/gbi.h"
#include "sys/camera.h"
#include "sys/rcp.h"
#include "sys/rsp_segment.h"
#include "sys/vi.h"
#include "sys/map.h"

extern u8 sBGPrimColourR;
extern u8 sBGPrimColourG;
extern u8 sBGPrimColourB;

extern u16 *gFramebufferEnd;

RECOMP_PATCH void rcp_clear_screen(Gfx **gdl, Mtx **mtx, s32 flags) {
    s32 viSize;
    s32 viWidth, viHeight;
    s32 ulx, uly, lrx, lry;
    s32 letterbox;

    viewport_get_full_rect(&ulx, &uly, &lrx, &lry);

    letterbox = camera_get_letterbox();

    viSize = vi_get_current_size();
    viWidth = GET_VIDEO_WIDTH(viSize);
    viHeight = GET_VIDEO_HEIGHT(viSize);

    // @recomp: remove hardcoded -1 width/height scissor offset
    gDPSetScissor((*gdl)++, G_SC_NON_INTERLACE, 0, 0, viWidth, viHeight);

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

    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, viWidth, SEGMENT_ADDR(SEGMENT_ZBUFFER, 0x0));

    if ((flags & CLEAR_ZBUFFER) != 0) {
        dl_set_fill_color(gdl, (GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0));

        gDPFillRectangle((*gdl)++, ulx, uly, lrx, lry);

        gDLBuilder->needsPipeSync = TRUE;
    }

    if (gDLBuilder->needsPipeSync) {
        gDLBuilder->needsPipeSync = FALSE;
        gDPPipeSync((*gdl)++);
    }

    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, viWidth, SEGMENT_ADDR(SEGMENT_FRAMEBUFFER, 0x0));

    if ((flags & CLEAR_COLOR) != 0 || letterbox != 0) {
        dl_set_fill_color(gdl, 
            (GPACK_RGBA5551(sBGPrimColourR, sBGPrimColourG, sBGPrimColourB, 1) << 16) 
                | GPACK_RGBA5551(sBGPrimColourR, sBGPrimColourG, sBGPrimColourB, 1));

        // @recomp: remove hardcoded -1 width/height offset
        if ((flags & CLEAR_COLOR) != 0) {
            gDPFillRectangle((*gdl)++, 0, 0, viWidth, viHeight);
            gDLBuilder->needsPipeSync = TRUE;
        } else if (letterbox != 0) {
            gDPFillRectangle((*gdl)++, 0, 0, viWidth, uly);
            gDLBuilder->needsPipeSync = TRUE;

            gDPFillRectangle((*gdl)++, 0, lry, viWidth, viHeight);
            gDLBuilder->needsPipeSync = TRUE;
        }
    }

    camera_apply_scissor(gdl);
}

void recomp_take_pause_screenshot(Gfx **gdl) {
    u32 viSize = vi_get_current_size();
    u32 viWidth = GET_VIDEO_WIDTH(viSize);
    u32 viHeight = GET_VIDEO_HEIGHT(viSize);

    // Use DP to copy framebuffer so RT64 uses the high res version when we go to draw it later
    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, gFramebufferEnd);
    gDPLoadTextureTile((*gdl)++, gFrontFramebuffer, G_IM_FMT_RGBA, G_IM_SIZ_16b, 
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

    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, viWidth, SEGMENT_FRAMEBUFFER << 24);
}

RECOMP_PATCH void draw_pause_screen_freeze_frame(Gfx** gdl) {
    s32 height;
    s32 width;

    width = 320;
    height = 240;

    gSPClearGeometryMode(*gdl, 0xFFFFFF);
    dl_apply_geometry_mode(gdl);
    
    gDPSetCombineMode(*gdl, G_CC_DECALRGBA, G_CC_DECALRGBA);
    dl_apply_combine(gdl);
   
    gDPSetOtherMode(*gdl, 
        G_AD_PATTERN | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | 
            G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_COPY | G_PM_NPRIMITIVE, 
        G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_NOOP2);
    dl_apply_other_mode(gdl);
  
    // @recomp: Let RT64 display the high res screenshot
    gDPLoadTextureTile((*gdl)++, gFramebufferEnd, G_IM_FMT_RGBA, G_IM_SIZ_16b, 
            width, height, 
            0, 0, width, height, 
            0, 
            G_TX_NOMIRROR | G_TX_WRAP | G_TX_NOMASK, 
            G_TX_NOMIRROR | G_TX_WRAP | G_TX_NOMASK,
            G_TX_NOMASK, G_TX_NOMASK,
            G_TX_NOLOD, G_TX_NOLOD);

    gSPTextureRectangle((*gdl)++, 0, 0, (width + 1) * 4, (height + 1) * 4, 0, 0, 0, 1 << 12, 1 << 10);

    gDLBuilder->needsPipeSync = 1;
}
