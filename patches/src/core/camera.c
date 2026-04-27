#include "patches.h"
#include "matrix_groups.h"

#include "sys/math.h"
#include "sys/camera.h"
#include "sys/map.h"
#include "sys/vi.h"

extern s32 gIsShadowTexActive;

extern Camera gCameras[CAMERA_COUNT];
extern s32 gCameraSelector;
extern SRT gCameraSRT;
extern s8 gUseAlternateCamera;
extern u32 gCameraYOffsetEnabled;
extern u16 gPerspNorm;
extern MtxF gViewProjMtx;
extern Mtx *gRSPMtxList;
extern MtxF gProjectionMtx;
extern MtxF gViewMtx;
extern MtxF gViewMtx2;
extern Mtx gRSPViewMtx2;
extern MtxF gAuxMtx;
extern Mtx *gRSPMatrices[30];

extern u32 gViewportMode;
extern s16 gLetterboxSize;
extern s16 gLetterboxTarget;
extern s32 gCameraSelector;

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

extern f32 fexp(f32 x, u32 iterations);

// Save these separately so we can avoid the precision issue noted below
// when the game restores the camera viewProj matrix
static Mtx *recomp_lastCamViewMtx;
static Mtx *recomp_lastCamProjMtx;
static MtxF *recomp_lastCamViewWorldOffsetMtx;
static s32 recomp_lastCamSelector;
static MtxF recomp_viewWorldOffsetResetMtx = {
    .m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    }
};
static MtxF *recomp_parentViewMtxs[30];
static _Bool recomp_skipCameraInterp = FALSE;

void recomp_set_skip_camera_interpolation(_Bool skip) {
    recomp_skipCameraInterp = skip;
    // if (skip) {
    //     recomp_printf("skip camera interp\n");
    // }
}

static void recomp_apply_camera_matrix_group(Gfx **gdl, s32 id) {
    if (recomp_skipCameraInterp) {
        gEXMatrixGroupSkipAll((*gdl)++, id, G_EX_NOPUSH, G_MTX_PROJECTION, G_EX_EDIT_NONE);
    } else {
        gEXMatrixGroupSimpleNormal((*gdl)++, id, G_EX_NOPUSH, G_MTX_PROJECTION, G_EX_EDIT_NONE);
    }
}

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

    matrix_from_srt_reversed(&gViewMtx, &gCameraSRT);
    matrix_concat(&gViewMtx, &gProjectionMtx, &gViewProjMtx);
    // @recomp: Not using long version of combined ViewProjMtx
    //matrix_f2l(&gViewProjMtx, *rspMtxs);

    //gRSPMtxList = *rspMtxs;

    // @recomp: Submit view and projection matrices separately to avoid a nasty float precision
    // issue with this game's projection matrix. After viewProj is converted from float -> long -> float
    // from here to RT64, m[0][2] and m[0][3], which are very small values, end up less precise. When
    // RT64 decomposes this matrix, this lack of precision is amplified greatly resulting in a very
    // wrong projection matrix on some frames.
    matrix_f2l(&gProjectionMtx, *rspMtxs);
    matrix_f2l(&gViewMtx, *rspMtxs + 1);

    recomp_lastCamProjMtx = (*rspMtxs + 0);
    recomp_lastCamViewMtx = (*rspMtxs + 1);
    recomp_lastCamSelector = gCameraSelector;

    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL((*rspMtxs)++), G_MTX_PROJECTION | G_MTX_LOAD);
    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL((*rspMtxs)++), G_MTX_PROJECTION | G_MTX_MUL);
    
    // @recomp: Tag camera matrix
    recomp_apply_camera_matrix_group(gdl, CAMERA_MTX_GROUP_ID_START + gCameraSelector);

    // @recomp: Submit the world view offset to RT64 so it can reconcile for frame interpolation
    if (!gIsShadowTexActive) {
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
    if (gCameraYOffsetEnabled != 0) {
        gCameraSRT.transl.y += camera->dty;
    }
    gCameraSRT.transl.z = z;
    
    matrix_from_srt(&gViewMtx2, &gCameraSRT);
    matrix_f2l(&gViewMtx2, &gRSPViewMtx2);

    gCameraSelector = prevCameraSel;

    i = 0;
    while (i < 30) {
        // @recomp:
        gRSPMatrices[i] = 0;
        recomp_parentViewMtxs[i] = NULL;
        i++;
    }
}

RECOMP_PATCH void setup_rsp_matrices_for_object(Gfx **gdl, Mtx **rspMtxs, Object *object)
{
    u8 ancestor;
    MtxF mtxf;
    Object *origObject;
    f32 oldScale;

    origObject = object;

    if (gRSPMatrices[object->matrixIdx] == NULL) {
        ancestor = FALSE;
        while (object != NULL) {
            if (object->parent == NULL) {
                object->srt.transl.x -= gWorldX;
                object->srt.transl.z -= gWorldZ;
            }

            oldScale = object->srt.scale;
            if (!(object->unkB0 & 0x8)) {
                object->srt.scale = 1.0f;
            }

            if (!ancestor) {
                matrix_from_srt(&MtxF_800a6a60, &object->srt);
            } else {
                matrix_from_srt(&mtxf, &object->srt);
                matrix_concat_4x3(&MtxF_800a6a60, &mtxf, &MtxF_800a6a60);
            }

            object->srt.scale = oldScale;

            if (object->parent == NULL) {
                object->srt.transl.x += gWorldX;
                object->srt.transl.z += gWorldZ;
            }

            object = object->parent;
            ancestor = TRUE;
        }

        // @recomp: Don't concat projection matrix, we'll submit that separately
        matrix_concat(&MtxF_800a6a60, &gViewMtx, &gAuxMtx2);
        matrix_f2l(&gAuxMtx2, *rspMtxs);
        gRSPMatrices[origObject->matrixIdx] = *rspMtxs;
        (*rspMtxs)++;

        // @recomp:
        // SRT recomp_viewWorldOffsetSRT = {
        //     .yaw = 0, .pitch = 0, .roll = 0,
        //     .flags = 0,
        //     .scale = 1.0f,
        //     .transl = { 
        //         .x = MtxF_800a6a60.m[3][0] + gWorldX, 
        //         .y = MtxF_800a6a60.m[3][1], 
        //         .z = MtxF_800a6a60.m[3][2] + gWorldZ
        //     }
        // };
        // matrix_from_srt((MtxF*)*rspMtxs, &recomp_viewWorldOffsetSRT);
        // recomp_parentViewMtxs[origObject->matrixIdx] = (MtxF*)*rspMtxs;
        // (*rspMtxs)++;
    }

    // @recomp: Submit projection and view separately to avoid precision issues.
    // See above notes in setup_rsp_camera_matrices
    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(recomp_lastCamProjMtx), G_MTX_PROJECTION | G_MTX_LOAD);
    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(gRSPMatrices[origObject->matrixIdx]), G_MTX_PROJECTION | G_MTX_MUL);
    
    // TODO: this gets janky when an object switches parent (or parent/no parent)
    //gEXSetViewMatrixFloat((*gdl)++, recomp_parentViewMtxs[origObject->matrixIdx]);
    gEXSetViewMatrixFloat((*gdl)++, recomp_lastCamViewWorldOffsetMtx);
 
    // @recomp: Tag parent obj camera matrix
    u32 objMtxGroup = (recomp_obj_get_matrix_group(origObject) * 16) + gCameraSelector + OBJ_CAMERA_MTX_GROUP_ID_START;
    recomp_apply_camera_matrix_group(gdl, objMtxGroup);
}

RECOMP_PATCH void camera_load_parent_projection(Gfx **gdl)
{
    // @recomp: Submit projection and view separately instead of [gRSPMtxList] to avoid precision issues.
    // See above notes in setup_rsp_camera_matrices
    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(recomp_lastCamProjMtx), G_MTX_PROJECTION | G_MTX_LOAD);
    gSPMatrix((*gdl)++, OS_K0_TO_PHYSICAL(recomp_lastCamViewMtx), G_MTX_PROJECTION | G_MTX_MUL);

    // @recomp: Submit the world view offset to RT64 so it can reconcile for frame interpolation
    gEXSetViewMatrixFloat((*gdl)++, recomp_lastCamViewWorldOffsetMtx);

    // @recomp: Restore camera matrix group
    recomp_apply_camera_matrix_group(gdl, CAMERA_MTX_GROUP_ID_START + recomp_lastCamSelector);
}

RECOMP_PATCH void viewport_get_full_rect(s32 *ulx, s32 *uly, s32 *lrx, s32 *lry)
{
    u32 wh = vi_get_current_size();
    u32 width = GET_VIDEO_WIDTH(wh);
    u32 height = GET_VIDEO_HEIGHT(wh);

    // @recomp: remove hardcoded -6/+6 y scissor offset
    *ulx = 0;
    *lrx = width;
    *uly = gLetterboxSize;
    *lry = height - gLetterboxSize;
}

RECOMP_PATCH void camera_apply_scissor(Gfx **gdl)
{
    s32 ulx, uly, lrx, lry;
    s32 wh;
    s32 centerX;
    s32 centerY;
    s32 width;
    s32 height;
    s32 padX;
    s32 padY;
    s32 mode;
    
    wh = vi_get_current_size();
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
