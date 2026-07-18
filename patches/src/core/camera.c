#include "patches.h"
#include "matrix_groups.h"

#include "sys/gfx/model_asm.h"
#include "sys/main.h"
#include "sys/math.h"
#include "sys/camera.h"
#include "sys/map.h"
#include "sys/vi.h"

extern s32 gIsShadowTexActive;

extern Camera gCameras[CAMERA_COUNT];
extern s32 gCameraSelector;
extern SRT gCameraSRT;
extern f32 gFovY;
extern f32 gAspect;
extern s8 gTriggerUseAlternateCamera;
extern s8 gUseAlternateCamera;
extern u32 gCameraYOffsetEnabled;
extern u16 gPerspNorm;
extern MtxF gViewProjMtx;
extern Mtx *gRSPMtxList;
extern MtxF gProjectionMtx;
extern MtxF gViewMtx;
extern MtxF gViewMtx2;
extern Mtx gRSPProjectionMtx;
extern Mtx gRSPViewMtx2;
extern MtxF gAuxMtx;
extern Mtx *gRSPMatrices[30];

extern u32 gViewportMode;
extern s16 gLetterboxSize;
extern s16 gLetterboxTarget;
extern f32 gProjectionScaleX;
extern s32 gCameraSelector;

extern f32 gNearPlane;
extern f32 gFarPlane;
extern s16 gFarPlaneTimer;
extern s16 gFarPlaneDuration;
extern f32 gFarPlaneStart;
extern f32 gFarPlaneTarget;
extern Camera gCameras[CAMERA_COUNT];
extern MatrixSlot gMatrixPool[100];
extern u32 gMatrixCount;
extern s8 gUseAlternateCamera;
extern s8 gMatrixIndex;
extern MtxF MtxF_800a6a60;
extern MtxF gAuxMtx2;

extern f32 cam_fexp(f32 x, u32 iterations);

static MtxF recomp_viewWorldOffsetResetMtx = {
    .m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    }
};
// Save these separately so we can avoid the precision issue noted below
// when the game restores the camera viewProj matrix
static Mtx *recomp_lastCamViewMtx;
static Mtx *recomp_lastCamProjMtx;
_Bool recomp_skipCameraInterp = FALSE;
static MatrixSlot *recomp_matrixPool; // Custom matrix pool with increased size

MtxF *recomp_objParentMtx = NULL;

void recomp_skip_camera_interp(void) {
    recomp_skipCameraInterp = TRUE;
}

RECOMP_PATCH void camSetupRSPMatrices(Gfx **gdl, Mtx **rspMtxs) {
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

    camUpdateCameraForObject(camera);

    if (gCameraSelector == 4) {
        map_func_80046B58(camera->tx, camera->ty, camera->tz);
    }

    x = camera->tx - gWorldX;
    y = camera->ty;
    z = camera->tz - gWorldZ;

    if (x > 32767.0f || -32767.0f > x || z > 32767.0f || -32767.0f > z) {
        // "Camera out of range: %d, (%.1f,%.1f,%.1f) (%.1f,%.1f)\n"
        return;
    }
    
    gCameraSRT.yaw = 0x8000 + camera->yaw;
    gCameraSRT.pitch = camera->pitch + camera->dpitch;
    gCameraSRT.roll = camera->roll;
    gCameraSRT.transl.x = -x;
    gCameraSRT.transl.y = -y;
    if (gCameraYOffsetEnabled != 0) {
        gCameraSRT.transl.y -= camera->dty;
    }
    gCameraSRT.transl.z = -z;

    mathRpyXyzMtx(&gViewMtx, &gCameraSRT);
    mathMtxCatF(&gViewMtx, &gProjectionMtx, &gViewProjMtx);
    // @recomp: Not using long version of combined ViewProjMtx
    //mathMtxF2L(&gViewProjMtx, *rspMtxs);

    //gRSPMtxList = *rspMtxs;

    // @recomp: Submit view and projection matrices separately to avoid a nasty float precision
    // issue with this game's projection matrix. After viewProj is converted from float -> long -> float
    // from here to RT64, m[0][2] and m[0][3], which are very small values, end up less precise. When
    // RT64 decomposes this matrix, this lack of precision is amplified greatly resulting in a very
    // wrong projection matrix on some frames.
    mathMtxF2L(&gProjectionMtx, *rspMtxs);
    mathMtxF2L(&gViewMtx, *rspMtxs + 1);

    recomp_lastCamProjMtx = (*rspMtxs + 0);
    recomp_lastCamViewMtx = (*rspMtxs + 1);

    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL((*rspMtxs)++), G_MTX_PROJECTION | G_MTX_LOAD);
    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL((*rspMtxs)++), G_MTX_PROJECTION | G_MTX_MUL);

    // @recomp: Projection slot changed, so the parent matrix is no longer valid (if any leaked over to this point)
    recomp_objParentMtx = NULL;
    
    // @recomp: Tag camera matrix
    if (recomp_skipCameraInterp || recomp_skipAllInterp) {
        gEXMatrixGroupSkipAll((*gdl)++, CAMERA_MTX_GROUP_ID_START + gCameraSelector, G_EX_NOPUSH, G_MTX_PROJECTION, G_EX_EDIT_NONE);
    } else {
        gEXMatrixGroupSimpleNormal((*gdl)++, CAMERA_MTX_GROUP_ID_START + gCameraSelector, G_EX_NOPUSH, G_MTX_PROJECTION, G_EX_EDIT_NONE);
    }

    // @recomp: Submit the world view offset to RT64 so it can reconcile for frame interpolation
    if (!gIsShadowTexActive) {
        SRT recomp_viewWorldOffsetSRT = {
            .yaw = 0, .pitch = 0, .roll = 0,
            .flags = 0,
            .scale = 1.0f,
            .transl = { .x = gWorldX, .y = 0, .z = gWorldZ }
        };
        mathYprXyzMtx((MtxF*)*rspMtxs, &recomp_viewWorldOffsetSRT);
        gEXSetViewMatrixFloat((*gdl)++, (MtxF*)(*rspMtxs)++);
    } else {
        // Disable for shadow ortho projection
        // TODO: should we be doing this?
        gEXSetViewMatrixFloat((*gdl)++, &recomp_viewWorldOffsetResetMtx);
    }

    gCameraSRT.yaw = -0x8000 - camera->yaw;
    gCameraSRT.pitch = -(camera->pitch + camera->dpitch);
    gCameraSRT.roll = -camera->roll;
    gCameraSRT.scale = 1.0f;
    gCameraSRT.transl.x = x;
    gCameraSRT.transl.y = y;
    if (gCameraYOffsetEnabled != 0) {
        gCameraSRT.transl.y += camera->dty;
    }
    gCameraSRT.transl.z = z;
    
    mathYprXyzMtx(&gViewMtx2, &gCameraSRT);
    mathMtxF2L(&gViewMtx2, &gRSPViewMtx2);

    gCameraSelector = prevCameraSel;

    i = 0;
    while (i < 30) {
        gRSPMatrices[i++] = NULL;
    }
}

RECOMP_PATCH void camSetupObjectSRTMatrix(Gfx **gdl, Mtx **rspMtxs, SRT *srt, f32 yScale, f32 unused, MtxF *outMtx) {
    MtxF *mtx;
    Mtx *fallbackMtx;

    mtx = outMtx == NULL ? &MtxF_800a6a60 : outMtx;

    srt->transl.x -= gWorldX;
    srt->transl.z -= gWorldZ;

    if (srt->transl.x > 32767.0f || -32767.0f > srt->transl.x || srt->transl.z > 32767.0f || -32767.0f > srt->transl.z) {
        fallbackMtx = mathIdentityMtxL();
        gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(fallbackMtx), G_MTX_LOAD | G_MTX_MODELVIEW);

        srt->transl.x += gWorldX;
        srt->transl.z += gWorldZ;
    } else {
        mathYprXyzMtx(mtx, srt);
        if (yScale != 1.0f) {
            mathSquashY(mtx, yScale);
        }

        // @recomp: Factor in parent matrix since we don't set it in the projection slot in recomp
        if (recomp_objParentMtx != NULL) {
            mathMtxCat4x3F(mtx, recomp_objParentMtx, mtx);
        }

        if (outMtx == NULL) {
            mathMtx4x3F2L(mtx, *rspMtxs);
            gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL((*rspMtxs)++), G_MTX_LOAD | G_MTX_MODELVIEW);
        } else {
            // Output matrix was given, so don't add matrix to the global rsp matrix list but add to
            // the matrix pool so it still gets converted to the long format later before the display
            // list is processed.
            gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(outMtx), G_MTX_LOAD | G_MTX_MODELVIEW);
            camAddMatrixToPool(outMtx, 1);
        }

        srt->transl.x += gWorldX;
        srt->transl.z += gWorldZ;
    }
}

RECOMP_PATCH void camSetupRSPMatricesForObject(Gfx **gdl, Mtx **rspMtxs, Object *object) {
    u8 ancestor;
    MtxF mtxf;
    Object *origObject;
    f32 oldScale;

    origObject = object;

    // @recomp: Storing the parent matrix in the projection slot is problematic for frame interpolation so
    //          instead we'll compute and save the parent matrix without changing the projection slot. Later on
    //          in (primarily) objprint code, the parent matrix will be factored in to the model view matrix on the CPU.
    //          We'll still use gRSPMatrices to cache the calculation done here.
    if (gRSPMatrices[object->matrixIdx] == NULL) {
        ancestor = FALSE;
        while (object != NULL) {
            if (object->parent == NULL) {
                object->srt.transl.x -= gWorldX;
                object->srt.transl.z -= gWorldZ;
            }

            oldScale = object->srt.scale;
            if (!(object->stateFlags & OBJSTATE_WORLD_MTX_IGNORE_SCALE)) {
                object->srt.scale = 1.0f;
            }

            if (!ancestor) {
                mathYprXyzMtx(&MtxF_800a6a60, &object->srt);
            } else {
                mathYprXyzMtx(&mtxf, &object->srt);
                mathMtxCat4x3F(&MtxF_800a6a60, &mtxf, &MtxF_800a6a60);
            }

            object->srt.scale = oldScale;

            if (object->parent == NULL) {
                object->srt.transl.x += gWorldX;
                object->srt.transl.z += gWorldZ;
            }

            object = object->parent;
            ancestor = TRUE;
        }

        mathMtxCatF(&MtxF_800a6a60, &gViewProjMtx, &gAuxMtx2);
        // @recomp: Don't store matrix concat'd with view-projection, just store the parent model matrix
        if (recomp_frameInterpActive) {
            bcopy(&MtxF_800a6a60, *rspMtxs, sizeof(MtxF));
        } else {
            mathMtxF2L(&MtxF_800a6a60, *rspMtxs);
        }
        gRSPMatrices[origObject->matrixIdx] = *rspMtxs;
        (*rspMtxs)++;
    }

    if (recomp_frameInterpActive) {
        // @recomp: Don't change projection matrix, just save the parent matrix for draws to use
        recomp_objParentMtx = (MtxF*)gRSPMatrices[origObject->matrixIdx];
    } else {
        // @recomp: Submit projection and view separately to avoid precision issues.
        // See above notes in setup_rsp_camera_matrices
        gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(recomp_lastCamProjMtx), G_MTX_PROJECTION | G_MTX_LOAD);
        gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(recomp_lastCamViewMtx), G_MTX_PROJECTION | G_MTX_MUL);
        gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(gRSPMatrices[origObject->matrixIdx]), G_MTX_PROJECTION | G_MTX_MUL);
        
        recomp_objParentMtx = NULL;
    }
}

RECOMP_PATCH void camLoadParentProjection(Gfx **gdl) {
    if (recomp_frameInterpActive) {
        // @recomp: Don't change projection matrix, just clear the parent matrix
        recomp_objParentMtx = NULL;
    } else {
        // @recomp: Submit projection and view separately instead of [gRSPMtxList] to avoid precision issues.
        // See above notes in setup_rsp_camera_matrices
        gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(recomp_lastCamProjMtx), G_MTX_PROJECTION | G_MTX_LOAD);
        gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(recomp_lastCamViewMtx), G_MTX_PROJECTION | G_MTX_MUL);
        
        recomp_objParentMtx = NULL;
    }
}

RECOMP_PATCH void camViewportGetFullRect(s32 *ulx, s32 *uly, s32 *lrx, s32 *lry) {
    u32 wh = viGetCurrentSize();
    u32 width = GET_VIDEO_WIDTH(wh);
    u32 height = GET_VIDEO_HEIGHT(wh);

    // @recomp: remove hardcoded -6/+6 y scissor offset
    *ulx = 0;
    *lrx = width;
    *uly = gLetterboxSize;
    *lry = height - gLetterboxSize;
}

RECOMP_PATCH void camApplyScissor(Gfx **gdl) {
    s32 ulx, uly, lrx, lry;
    s32 wh;
    s32 centerX;
    s32 centerY;
    s32 width;
    s32 height;
    s32 padX;
    s32 padY;
    s32 mode;
    
    wh = viGetCurrentSize();
    height = GET_VIDEO_HEIGHT(wh) & 0xFFFF;
    width = GET_VIDEO_WIDTH(wh);
    
    mode = gViewportMode;
    
    if (mode != 0)
    {
        if (mode == 2) {
            mode = 3;
        }

        lrx = ulx = 0;
        lry = uly = 0;
        lrx += width;\
        lry += height;

        centerX = width >> 1;
        centerY = height >> 1;
        padX = width >> 8;
        padY = height >> 7;

        switch (mode)
        {
        case 1:
            if (gCameraSelector == 0) {
                lry = centerY - padY;
            } else {
                lry = height - padY;
                uly = centerY + padY;
            }
            break;
        case 2:
            if (gCameraSelector == 0) {
                lrx = centerX - padX;
                ulx = 0;
            } else {
                lrx = width - padX;
                ulx = centerX + padX;
            }
            break;
        case 3:
            switch (gCameraSelector)
            {
            case 0:
                lrx = centerX - padX;\
                lry = centerY - padY;
                break;
            case 1:
                ulx = centerX + padX;
                lrx = width - padX;
                lry = centerY - padY;
                break;
            case 2:
                uly = centerY + padY;
                lrx = centerX - padX;
                lry = height - padY;
                break;
            case 3:
                ulx = centerX + padX;\
                uly = centerY + padY;
                lry = height - padY;
                lrx = width - padX;
                break;
            }
            break;
        }
    }
    else
    {
        ulx = 0;
        uly = 0;
        lrx = width;\
        lry = height;

        // @recomp: remove hardcoded -6/+6 y scissor offset
        uly += gLetterboxSize;
        lry -= gLetterboxSize;
    }

    gDPSetScissor((*gdl)++, 0, ulx, uly, lrx, lry);
}

RECOMP_PATCH void camInit(void) {
    s32 i;
    u32 stat;

    for (i = 0; i < 12; i++) {
        gCameraSelector = i;
        camReset(200, 200, 200, 0, 0, 180);
    }

    gTriggerUseAlternateCamera = 0;
    gUseAlternateCamera = 0;
    gCameraSelector = 0;
    gViewportMode = 0;
    gCameraYOffsetEnabled = 0;
    gMatrixCount = 0;
    gMatrixIndex = 0;
    gFarPlane = 10000.0f;
    gFarPlaneTimer = 0;
    // @recomp: Skip ROM check
    // gAntiPiracyFlag = 0;

    // WAIT_ON_IOBUSY(stat);

    // // 0xB0000578 is a direct read from the ROM as opposed to RAM
    // if (((D_B0000578 & 0xFFFF) & 0xFFFF) != 0x8965) {
    //     gAntiPiracyFlag = TRUE;
    // }

    gAspect = 1.3333334f;
    gFovY = 60.0f;

    guPerspectiveF(gProjectionMtx.m, &gPerspNorm, gFovY, gAspect, gNearPlane, gFarPlane, 1.0f);
    mathMtxF2L(&gProjectionMtx, &gRSPProjectionMtx);

    gProjectionScaleX = gProjectionMtx.m[0][0];
    gLetterboxSize = 0;
    gLetterboxTarget = 0;

    // @recomp: Initialize custom matrix pool
    recomp_matrixPool = recomp_alloc(sizeof(MatrixSlot) * 400); // 4x size
}

RECOMP_PATCH void camAddMatrixToPool(MtxF *mf, s32 count) {
    // @recomp: Use custom matrix pool
    recomp_matrixPool[gMatrixCount].mtx = (Mtx_MtxF*)mf;
    recomp_matrixPool[gMatrixCount++].count = count;

    if (gMatrixCount == 400) {
        recomp_eprintf("Camera matrix pool overflow!\n");
    }
}

RECOMP_PATCH void camTick(void) {
    s32 pad;
    f32 lerpFactor;
    Camera *camera;
    f32 dampFactor;

    gLetterboxSize = gLetterboxTarget;

    if (gFarPlaneTimer != 0) {
        gFarPlaneTimer -= gUpdateRate;

        if (gFarPlaneTimer < 0) {
            gFarPlaneTimer = 0;
        }

        lerpFactor = ((f32)gFarPlaneTimer / (f32)gFarPlaneDuration);
        gFarPlane = (gFarPlaneStart - gFarPlaneTarget) * lerpFactor + gFarPlaneTarget;
    }

    // @recomp: Use custom matrix pool
    recomp_matrixPool[gMatrixCount].count = -1;
    convert_mtxf_to_mtx_in_pool(recomp_matrixPool);

    gMatrixCount = 0;
    gMatrixIndex = 0;

    if (gUseAlternateCamera) {
        gCameraSelector += 4;
    }
    
    camera = &gCameras[gCameraSelector];

    if (camera->shakeMode == 0) {
        camera->shakeCooldown--;

        while (camera->shakeCooldown < 0) {
            camera->dty = -camera->dty * 0.89999998f;

            camera->shakeCooldown++;
        }
    } else if (camera->shakeMode == 1) {
        dampFactor = cam_fexp(-camera->shakeDamping * camera->shakeTime, 20);
        lerpFactor = mathCosfInterp(camera->shakeFrequency * 65535.0f * camera->shakeTime);
        lerpFactor *= camera->shakeAmplitude * dampFactor;

        camera->dty = lerpFactor;

        if (camera->dty < 0.1f && -0.1f < camera->dty) {
            camera->shakeMode = -1;
            camera->dty = 0.0f;
        }

        camera->shakeTime += gUpdateRateF / 60.0f;
    }

    // @recomp: Reset camera interp skip
    recomp_skipCameraInterp = FALSE;
}
