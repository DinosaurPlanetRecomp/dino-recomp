#include "patches.h"
#include "sys/math.h"
#include "sys/camera.h"
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

// Save these separately so we can avoid the precision issue noted below
// when the game restores the camera viewProj matrix
static Mtx *recomp_lastCamViewMtx;
static Mtx *recomp_lastCamProjMtx;

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
}

