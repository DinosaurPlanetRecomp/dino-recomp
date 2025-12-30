#include "patches.h"

#include "sys/math.h"
#include "sys/gfx/gx.h"
#include "sys/camera.h"
#include "sys/main.h"
#include "functions.h"
#include "variables.h"

extern Camera gCameras[CAMERA_COUNT];
extern s32 gCameraSelector;
extern SRT gCameraSRT;
extern s8 gUseAlternateCamera;
extern u32 UINT_800a6a54;
extern u16 gPerspNorm;
extern MtxF gViewProjMtx;
extern Mtx *gRSPMtxList;
extern MtxF gProjectionMtx;
extern MtxF gViewMtx;
extern MtxF gViewMtx2;
extern Mtx gRSPViewMtx2;
extern MtxF gAuxMtx;
extern Mtx *gRSPMatrices[30];

extern u32 UINT_800a66f8;
extern s16 SHORT_8008c524;
extern s16 SHORT_8008c528;
extern s32 gCameraSelector;

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

extern f32 fexp(f32 x, u32 iterations);

// Save these separately so we can avoid the precision issue noted below
// when the game restores the camera viewProj matrix
static Mtx *recomp_lastCamViewMtx;
static Mtx *recomp_lastCamProjMtx;
static MtxF *recomp_lastCamViewWorldOffsetMtx;
static MtxF recomp_viewWorldOffsetResetMtx = {
    .m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    }
};

RECOMP_PATCH void setup_rsp_camera_matrices(Gfx **gdl, Mtx **rspMtxs) {
    s32 prevCameraSel;
    f32 x,y,z;
    s32 i;
    Camera *camera;

    gSPPerspNormalize((*gdl)++, gPerspNorm);

    prevCameraSel = gCameraSelector;
    
    if (gUseAlternateCamera) {
        gCameraSelector += 4;
    }
 
    camera = &gCameras[gCameraSelector];

    update_camera_for_object(camera);

    if (gCameraSelector == 4) {
        map_func_80046B58(camera->tx, camera->ty, camera->tz);
    }

    x = camera->tx - gWorldX;
    y = camera->ty;
    z = camera->tz - gWorldZ;

    if (x > 32767.0f || -32767.0f > x || z > 32767.0f || -32767.0f > z) {
        return;
    }
    
    gCameraSRT.yaw = 0x8000 + camera->yaw;
    gCameraSRT.pitch = camera->pitch + camera->dpitch;
    gCameraSRT.roll = camera->roll;
    gCameraSRT.transl.x = -x;
    gCameraSRT.transl.y = -y;
    if (UINT_800a6a54 != 0) {
        gCameraSRT.transl.y -= camera->dty;
    }
    gCameraSRT.transl.z = -z;

    matrix_from_srt_reversed(&gViewMtx, &gCameraSRT);
    matrix_concat(&gViewMtx, &gProjectionMtx, &gViewProjMtx);
    matrix_f2l(&gViewProjMtx, *rspMtxs);

    gRSPMtxList = *rspMtxs;

    // @recomp: Submit view and projection matrices separately to avoid a nasty float precision
    // issue with this game's projection matrix. After viewProj is converted from float -> long -> float
    // from here to RT64, m[0][2] and m[0][3], which are very small values, end up less precise. When
    // RT64 decomposes this matrix, this lack of precision is amplified greatly resulting in a very
    // wrong projection matrix on some frames.
    matrix_f2l(&gProjectionMtx, *rspMtxs);
    matrix_f2l(&gViewMtx, *rspMtxs + 1);

    recomp_lastCamProjMtx = (*rspMtxs + 0);
    recomp_lastCamViewMtx = (*rspMtxs + 1);

    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL((*rspMtxs)++), G_MTX_PROJECTION | G_MTX_LOAD);
    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL((*rspMtxs)++), G_MTX_PROJECTION | G_MTX_MUL);
    
    // @recomp: Submit the world view offset to RT64 so it can reconcile for frame interpolation
    if (!gSomeVideoFlag) {
        SRT recomp_viewWorldOffsetSRT = {
            .yaw = 0, .pitch = 0, .roll = 0,
            .flags = 0,
            .scale = 1.0f,
            .transl = { .x = gWorldX, .y = 0, .z = gWorldZ }
        };
        matrix_from_srt((MtxF*)*rspMtxs, &recomp_viewWorldOffsetSRT);
        recomp_lastCamViewWorldOffsetMtx = (MtxF*)(*rspMtxs + 0);
        gEXSetViewMatrixFloat((*gdl)++, (MtxF*)(*rspMtxs)++);
    } else {
        // Disable for shadow ortho projection
        recomp_lastCamViewWorldOffsetMtx = &recomp_viewWorldOffsetResetMtx;
        gEXSetViewMatrixFloat((*gdl)++, &recomp_viewWorldOffsetResetMtx);
    }

    gCameraSRT.yaw = -0x8000 - camera->yaw;
    gCameraSRT.pitch = -(camera->pitch + camera->dpitch);
    gCameraSRT.roll = -camera->roll;
    gCameraSRT.scale = 1.0f;
    gCameraSRT.transl.x = x;
    gCameraSRT.transl.y = y;
    if (UINT_800a6a54 != 0) {
        gCameraSRT.transl.y += camera->dty;
    }
    gCameraSRT.transl.z = z;
    
    matrix_from_srt(&gViewMtx2, &gCameraSRT);
    matrix_f2l(&gViewMtx2, &gRSPViewMtx2);

    gCameraSelector = prevCameraSel;

    i = 0;
    while (i < 30) {
        gRSPMatrices[i++] = 0;
    }
}

RECOMP_PATCH void func_80004224(Gfx **gdl)
{
    // @recomp: Submit projection and view separately instead of [gRSPMtxList] to avoid precision issues
    // See above notes in setup_rsp_camera_matrices
    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(recomp_lastCamProjMtx), G_MTX_PROJECTION | G_MTX_LOAD);
    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(recomp_lastCamViewMtx), G_MTX_PROJECTION | G_MTX_MUL);

    // @recomp: Submit the world view offset to RT64 so it can reconcile for frame interpolation
    gEXSetViewMatrixFloat((*gdl)++, recomp_lastCamViewWorldOffsetMtx);
}

RECOMP_PATCH void func_80002130(s32 *ulx, s32 *uly, s32 *lrx, s32 *lry)
{
    u32 wh = vi_get_current_size();
    u32 width = GET_VIDEO_WIDTH(wh);
    u32 height = GET_VIDEO_HEIGHT(wh);

    // @recomp: remove hardcoded -6/+6 y scissor offset
    *ulx = 0;
    *lrx = width;
    *uly = SHORT_8008c524;
    *lry = height - SHORT_8008c524;
}

RECOMP_PATCH void func_80002490(Gfx **gdl)
{
    s32 ulx, uly, lrx, lry;
    s32 wh = vi_get_current_size();
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

RECOMP_PATCH void camera_tick() {
    Camera *camera;
    f32 var4;
    f32 var5;

    SHORT_8008c524 = SHORT_8008c528;
    // @recomp: Add back +6 offset removed from other scissor patches
    // This is necessary for the cinematic top/bottom black bars during cutscnes to be the correct size
    // TODO: theres probably a better place for this fix
    if (SHORT_8008c528 != 0) {
        SHORT_8008c524 += (s32)(((f32)SHORT_8008c528 / 30.0f) * 6);
    }

    if (D_8008C518 != 0) {
        D_8008C518 -= gUpdateRate;

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

        camera->unk38 += gUpdateRateF / 60.0f;
    }
}
