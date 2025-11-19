#include "patches.h"
#include "rt64_extended_gbi.h"

#include "PR/gbi.h"
#include "sys/memory.h"
#include "sys/gfx/map.h"
#include "functions.h"

#include "recomp/dlls/engine/20_screens_recomp.h"

extern s32 sLoadedScreenNo;
extern s32 sRenderScreen;
extern s32 sLoadedScreenByteLength;
extern u16 *sLoadedScreen;

RECOMP_PATCH void screens_draw(Gfx **gdl) {
    s32 width;
    s32 yPos;
    s32 height;
    s32 chunkSize;
    u16 *ptr;

    chunkSize = 2;

    if (!sRenderScreen) {
        if (sLoadedScreen != NULL) {
            mmFree(sLoadedScreen);
            sLoadedScreen = NULL;
            sLoadedScreenByteLength = 0;
            sLoadedScreenNo = -1;
        }
    } else if (sLoadedScreen != NULL) {
        sRenderScreen = FALSE;

        width = sLoadedScreen[0];
        height = sLoadedScreen[1];

        ptr = sLoadedScreen;
        ptr += 8;

        // @recomp start
        // Fill behind screen with black since the screen texture won't fill widescreen (it's a 4:3 image)
        gDPSetCombineMode(*gdl, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        dl_apply_combine(gdl);

        gDPSetOtherMode(*gdl, 
            G_AD_PATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | 
                G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE, 
            G_AC_NONE | G_ZS_PIXEL | G_RM_CLD_SURF | G_RM_CLD_SURF2);
        dl_apply_other_mode(gdl);

        dl_set_prim_color(gdl, 0, 0, 0, 255);
        gDPFillRectangle((*gdl)++, 0, 0, 320, 240);
        
        gDLBuilder->needsPipeSync = TRUE;
        // @recomp end

        gDPSetCombineLERP((*gdl), TEXEL0, 0, SCALE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, SCALE, 0, 0, 0, 0, TEXEL0);
        dl_apply_combine(gdl);

        gDPSetOtherMode((*gdl), 
            G_AD_PATTERN | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_NONE | G_CYC_COPY | G_PM_NPRIMITIVE, 
            G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_NOOP2);
        dl_apply_other_mode(gdl);

        yPos = 0;
        do {
            gDPLoadTextureBlockS((*gdl)++,
                /*timg*/    OS_K0_TO_PHYSICAL(ptr),
                /*fmt*/     G_IM_FMT_RGBA,
                /*siz*/     G_IM_SIZ_16b,
                /*width*/   width,
                /*height*/  chunkSize,
                /*pal*/     0,
                /*cms*/     G_TX_CLAMP,
                /*cmt*/     G_TX_CLAMP,
                /*masks*/   G_TX_NOMASK,
                /*maskt*/   G_TX_NOMASK,
                /*shifts*/  G_TX_NOLOD,
                /*shiftt*/  G_TX_NOLOD);

            // @recomp: Use EX tex rectangle to center the screen texture and stop it from
            // stretching out when playing in widescreen.
            gEXTextureRectangle((*gdl)++,
                /*lorigin*/ G_EX_ORIGIN_CENTER,
                /*rorigin*/ G_EX_ORIGIN_CENTER,
                /*ulx*/ (-320 / 2) * 4,
                /*uly*/ yPos << 2,
                /*lrx*/ (width - (320 / 2)) * 4,
                /*lry*/ (yPos + 1) << 2,
                /*tile*/G_TX_RENDERTILE,
                /*s*/   0,
                /*t*/   0,
                /*dsdx*/1 << 12,
                /*dtdy*/1 << 10);

            gDLBuilder->needsPipeSync = TRUE;

            ptr += width * chunkSize;
            yPos += chunkSize;
        } while (yPos < height);
    }
}
