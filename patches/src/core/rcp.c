#include "patches.h"

#include "PR/gbi.h"
#include "sys/gfx/gx.h"
#include "sys/gfx/map.h"
#include "sys/camera.h"
#include "functions.h"

extern u8 sBGPrimColourR;
extern u8 sBGPrimColourG;
extern u8 sBGPrimColourB;

RECOMP_PATCH void func_80037A14(Gfx **gdl, Mtx **mtx, s32 param3) {
    s32 resolution;
    s32 resWidth, resHeight;
    s32 ulx, uly, lrx, lry;
    s32 var1;

    func_80002130(&ulx, &uly, &lrx, &lry);

    var1 = func_80004A4C();

    resolution = vi_get_current_size();
    resWidth = GET_VIDEO_WIDTH(resolution);
    resHeight = GET_VIDEO_HEIGHT(resolution);

    // @recomp: remove hardcoded -1 width/height scissor offset
    gDPSetScissor((*gdl)++, G_SC_NON_INTERLACE, 0, 0, resWidth, resHeight);

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

    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, resWidth, 0x02000000);

    if ((param3 & 2) != 0) {
        dl_set_fill_color(gdl, (GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0));

        gDPFillRectangle((*gdl)++, ulx, uly, lrx, lry);

        gDLBuilder->needsPipeSync = TRUE;
    }

    if (gDLBuilder->needsPipeSync) {
        gDLBuilder->needsPipeSync = FALSE;
        gDPPipeSync((*gdl)++);
    }

    gDPSetColorImage((*gdl)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, resWidth, 0x01000000);

    if ((param3 & 1) != 0 || var1 != 0) {
        dl_set_fill_color(gdl, 
            (GPACK_RGBA5551(sBGPrimColourR, sBGPrimColourG, sBGPrimColourB, 1) << 16) 
                | GPACK_RGBA5551(sBGPrimColourR, sBGPrimColourG, sBGPrimColourB, 1));

        // @recomp: remove hardcoded -1 width/height offset
        if ((param3 & 1) != 0) {
            gDPFillRectangle((*gdl)++, 0, 0, resWidth, resHeight);
            gDLBuilder->needsPipeSync = TRUE;
        } else if (var1 != 0) {
            gDPFillRectangle((*gdl)++, 0, 0, resWidth, uly);
            gDLBuilder->needsPipeSync = TRUE;

            gDPFillRectangle((*gdl)++, 0, lry, resWidth, resHeight);
            gDLBuilder->needsPipeSync = TRUE;
        }
    }

    func_80002490(gdl);
}
