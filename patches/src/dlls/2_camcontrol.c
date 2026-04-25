#include "patches.h"

#include "dlls/engine/2_camcontrol.h"
#include "sys/curves.h"
#include "sys/main.h"
#include "sys/map.h"

#include "recomp/dlls/engine/2_camcontrol_recomp.h"

/*0x11C*/ extern CamControl_Data* sCamData;
/*0x1C0*/ extern f32 sFov;
/*0x1D4*/ extern s16 sLetterboxHeight;  //Controls the letterboxing at the top/bottom of frame

RECOMP_PATCH void CamControl_update_camera(CamControl_Data* camData) {
    Camera* camera;
    f32 tValue;
    Vec4f ease;

    set_camera_selector(0);
    camera = get_main_camera();
    camera->srt.yaw = camData->srt.yaw;
    camera->srt.pitch = camData->srt.pitch;
    camera->srt.roll = camData->srt.roll;
    camera->srt.transl.x = camData->srt.transl.x;
    camera->srt.transl.y = camData->srt.transl.y;
    camera->srt.transl.z = camData->srt.transl.z;

    sFov = camData->fov;

    if (camData->tValue > 0.0f) {
        camData->tValue -= camData->tSpeed * gUpdateRateF;

        //Get Bezier-eased tValue for interpolation
        ease.w = 0.0f;
        ease.z = 0.0f;
        ease.x = 0.0f;
        ease.y = 1.0f;
        tValue = 1.0f - func_80004C5C(&ease, camData->tValue, 0);

        //Linear interpolation (position)
        if (camData->lerpFlags & CamControl_Ease_X) {
            camera->srt.transl.x = camData->easeStartX + (camera->srt.transl.x - camData->easeStartX)*tValue;
        }
        if (camData->lerpFlags & CamControl_Ease_Y) {
            camera->srt.transl.y = camData->easeStartY + (camera->srt.transl.y - camData->easeStartY)*tValue;
        }
        if (camData->lerpFlags & CamControl_Ease_Z) {
            camera->srt.transl.z = camData->easeStartZ + (camera->srt.transl.z - camData->easeStartZ)*tValue;
        }
        
        //Linear interpolation (rotation)
        if (camData->lerpFlags & CamControl_Ease_Yaw) {
            camData->dYaw = camData->easeStartYaw - (camera->srt.yaw & 0xFFFF);
            CIRCLE_WRAP(camData->dYaw)
            camera->srt.yaw = camData->easeStartYaw - ((s16)(camData->dYaw * tValue));
        }
        if (camData->lerpFlags & CamControl_Ease_Pitch) {
            camData->dPitch = camData->easeStartPitch - (camera->srt.pitch & 0xFFFF);
            CIRCLE_WRAP(camData->dPitch)
            camera->srt.pitch = camData->easeStartPitch - ((s16)(camData->dPitch * tValue));
        }
        if (camData->lerpFlags & CamControl_Ease_Roll) {
            camData->dRoll = camData->easeStartRoll - (camera->srt.roll & 0xFFFF);
            CIRCLE_WRAP(camData->dRoll)
            camera->srt.roll = camData->easeStartRoll - ((s16)(camData->dRoll * tValue));
        }
    }
    
    //Change FOV
    if (camera_get_fov() != sFov) {
        camera_set_fov(sFov);
    }
    
    update_camera_for_object(camera);
    map_func_80046B58(camera->tx, camera->ty, camera->tz);

    //Update camera letterboxing
    sLetterboxHeight = camera_get_letterbox();
    // @recomp: Add back +6 offset removed from other scissor patches.
    //          This is necessary for the cinematic top/bottom black bars during cutscnes to be the correct size.
    s32 letterboxGoal = camData->letterboxGoal == 0 ? 0 : camData->letterboxGoal + 6;
    if (sLetterboxHeight != letterboxGoal) {
        if (sLetterboxHeight < letterboxGoal) {
            sLetterboxHeight += camData->letterboxSpeed * (s32) gUpdateRateF;
            if (letterboxGoal < sLetterboxHeight) {
                sLetterboxHeight = letterboxGoal;
            }
        } else {
            sLetterboxHeight -= camData->letterboxSpeed * (s32) gUpdateRateF;
            if (sLetterboxHeight < letterboxGoal) {
                sLetterboxHeight = letterboxGoal;
            }
        }
        camera_set_letterbox(sLetterboxHeight);
    }
    
    camData->letterboxGoal = 0;
}

RECOMP_PATCH void CamControl_set_letterbox_goal(s32 height, s32 startAtGoal) {
    if (sCamData->letterboxGoal < height) {
        sCamData->letterboxGoal = height;
        sCamData->letterboxSpeed = 1;
        
        if (startAtGoal) {
            // @recomp: Add back +6 offset removed from other scissor patches.
            //          Same patch as in CamControl_update_camera.
            camera_set_letterbox(height == 0 ? 0 : height + 6);
        }
    }
}
