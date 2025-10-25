#include "common.h"
#include "patches.h"

extern u32 UINT_800a66f8;
extern s16 SHORT_8008c524;
extern s16 SHORT_8008c528;
extern s32 gCameraSelector;

extern u8 D_800917A0;
extern u8 D_800917A4;
extern u8 D_800917A8;

RECOMP_PATCH void func_80002130(s32 *ulx, s32 *uly, s32 *lrx, s32 *lry)
{
    u32 wh = get_some_resolution_encoded();
    u32 width = wh & 0xffff;
    u32 height = wh >> 16;

    // @recomp: remove hardcoded -6/+6 y scissor offset
    *ulx = 0;
    *lrx = width;
    *uly = SHORT_8008c524;
    *lry = height - SHORT_8008c524;
}

RECOMP_PATCH void func_80002490(Gfx **gdl)
{
    s32 ulx, uly, lrx, lry;
    s32 wh = get_some_resolution_encoded();
    s32 width = wh & 0xffff;
    s32 height = wh >> 16;

    if (UINT_800a66f8 != 0)
    {
        // This if block is from non-matching code and I don't trust it.
        // It shouldn't ever run tho...
        recomp_eprintf("func_80002490: UINT_800a66f8 = %d", UINT_800a66f8);

        s32 centerX;
        s32 centerY;
        s32 padX;
        s32 padY;

        u32 mode = UINT_800a66f8;
        if (mode == 2) {
            mode = 3;
        }

        ulx = 0;
        uly = 0;
        lrx = width;
        lry = height;
        centerX = width >> 1;
        centerY = height >> 1;
        padX = width >> 8;
        padY = height >> 7;

        switch (mode)
        {
        case 1:
            lry -= padY;
            if (gCameraSelector == 0) {
                lry = centerY - padY;
            } else {
                uly = centerY + padY;
            }
            break;
        case 2:
            lrx -= padX;
            if (gCameraSelector == 0) {
                lrx = centerX - padX;
            } else {
                lrx = centerX + padX;
            }
            break;
        case 3:
            switch (gCameraSelector)
            {
            case 0:
                lry = centerY - padY;
                lrx = centerX - padX;
                break;
            case 1:
                ulx = centerX + padX;
                lrx -= padX;
                lry = centerY - padY;
                break;
            case 2:
                uly = centerY + padY;
                lrx = centerX - padX;
                lry -= padY;
                break;
            case 3:
                uly = centerY + padY;
                ulx = centerX + padX;
                lry -= padY;
                lrx -= padX;
                break;
            }
            break;
        }
    }
    else
    {
        // @recomp: remove hardcoded -6/+6 y scissor offset
        ulx = 0;
        lrx = width;
        lry = height - SHORT_8008c524;
        uly = SHORT_8008c524;
    }

    gDPSetScissor((*gdl)++, 0, ulx, uly, lrx, lry);
}

RECOMP_PATCH void func_80037A14(Gfx **gdl, Mtx **mtx, s32 param3) {
    s32 resolution;
    s32 resWidth, resHeight;
    s32 ulx, uly, lrx, lry;
    s32 var1;

    func_80002130(&ulx, &uly, &lrx, &lry);

    var1 = func_80004A4C();

    resolution = get_some_resolution_encoded();
    resWidth = RESOLUTION_WIDTH(resolution);
    resHeight = RESOLUTION_HEIGHT(resolution);

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
            (GPACK_RGBA5551(D_800917A0, D_800917A4, D_800917A8, 1) << 16) 
                | GPACK_RGBA5551(D_800917A0, D_800917A4, D_800917A8, 1));

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

extern f32 gFarPlane;
extern s16 D_8008C518;
extern s16 D_8008C51C;
extern f32 D_800A6270;
extern f32 D_800A6274;
extern Camera gCameras[CAMERA_COUNT];
extern MatrixSlot gMatrixPool[100];
extern u32 gMatrixCount;
extern s8 gUseAlternateCamera;
extern s8 gMatrixIndex;

f32 fexp(f32 x, u32 iterations);

RECOMP_PATCH void camera_tick() {
    Camera *camera;
    f32 var4;
    f32 var5;

    SHORT_8008c524 = SHORT_8008c528;
    // @recomp: Add back +6 offset removed from other scissor patches
    // This is necessary for the cinematic top/bottom black bars during cutscnes to be the correct size
    // TODO: theres probably a better place for this fix, this kinda screws up the animation for the black bars
    if (SHORT_8008c524 != 0) {
        SHORT_8008c524 += 6;
    }

    if (D_8008C518 != 0) {
        D_8008C518 -= delayByte;

        if (D_8008C518 < 0) {
            D_8008C518 = 0;
        }

        gFarPlane = (D_800A6270 - D_800A6274) * ((f32)D_8008C518 / (f32)D_8008C51C) + D_800A6274;
    }

    gMatrixPool[gMatrixCount].count = -1;

    convert_mtxf_to_mtx_in_pool(gMatrixPool);

    gMatrixCount = 0;
    gMatrixIndex = 0;

    if (gUseAlternateCamera) {
        gCameraSelector += 4;
    }
    
    camera = &gCameras[gCameraSelector];

    if (camera->unk5D == 0) {
        camera->unk5C--;

        while (camera->unk5C < 0) {
            camera->dty = -camera->dty * 0.89999998f;

            camera->unk5C++;
        }
    } else if (camera->unk5D == 1) {
        var4 = fexp(-camera->unk3C * camera->unk38, 20);
        var5 = fcos16_precise(camera->unk34 * 65535.0f * camera->unk38);
        var5 *= camera->unk30 * var4;

        camera->dty = var5;

        if (camera->dty < 0.1f && -0.1f < camera->dty) {
            camera->unk5D = -1;
            camera->dty = 0.0f;
        }

        camera->unk38 += delayFloat / 60.0f;
    }
}
