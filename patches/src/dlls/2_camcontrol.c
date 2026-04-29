#include "patches.h"
#include "matrix_groups.h"

#include "dlls/engine/2_camcontrol.h"
#include "game/objects/interaction_arrow.h"
#include "game/objects/object.h"
#include "sys/curves.h"
#include "sys/main.h"
#include "sys/map.h"
#include "sys/menu.h"
#include "sys/memory.h"
#include "sys/voxmap.h"
//#include "sys/print.h"
#include "sys/objects.h"
#include "dll.h"

#include "recomp/dlls/engine/2_camcontrol_recomp.h"

/*0x11C*/ extern CamControl_Data* sCamData;
/*0x174*/ extern CamControl_Module* sActiveModule; //Active module: the camera module DLL currently in use
/*0x178*/ extern s32 sActiveID;          //Active module: DLL ID
/*0x17C*/ extern s32 sActiveLoadedIndex; //Active module: The loaded index of the camera module DLL currently in use
/*0x180*/ extern s32 sNextID;            //Ease (End Module): DLL ID of next module (when switching camera DLLs)
/*0x184*/ extern s32 sActiveFree;        //Active module: free setting
/*0x188*/ extern s32 sActiveSetupVal;    //Active Module: arg1 for DLL's setup function (TO-DO: figure this out)
/*0x18C*/ extern UNK_PTR* sCamAction;    //Often a CameraAction*, but sometimes points to a shorter 8-byte struct? (TO-DO: figure this out)
/*0x190*/ extern u8 sEaseSetupNeeded;    //Boolean, starts a new camera ease
/*0x194*/ extern s32 sEaseDuration;      //Duration of easing between camera modules, in frames
/*0x198*/ extern u8 sEaseFlags;          //Which components to lerp during ease (x/y/z/yaw/pitch/roll)
/*0x19C*/ extern s32 sPreviousID;        //Ease (Start Module): DLL ID of previous module
/*0x1A0*/ extern s32 sPreviousFree;      //Ease (Start Module): free setting for previous module
/*0x1A4*/ extern s32 sPreviousSetupVal;  //Ease (Start Module): arg1 for previous module's setup function
/*0x1C0*/ extern f32 sFov;
/*0x1D4*/ extern s16 sLetterboxHeight;  //Controls the letterboxing at the top/bottom of frame

extern void CamControl_update_camera(CamControl_Data* arg0);
extern void CamControl_replace_active_module(u16 camDLLID, CameraAction* camAction);
extern void CamControl_store_player_coords(Object* arg0);
extern void CamControl_average_player_speed(CamControl_Data* arg0, Object* arg1);
extern void CamControl_restore_player_coords(Object* obj);
extern Object* CamControl_find_highlight_object(CamControl_Data* arg0, Object* arg1);

static f32 recomp_get_camera_jump_threshold(f32 cameraSpeed) {
    static _Bool lastInSeq = FALSE;

    // TODO: these values are way too arbitrary
    Object *player = get_player();
    _Bool inSeq = player != NULL && (player->stateFlags & OBJSTATE_IN_SEQ);
    if (inSeq != lastInSeq) {
        lastInSeq = inSeq;
        //recomp_printf("seq switch\n");
        return -1.0f;
    }
    if (inSeq) {
        // Player in sequence
        return 7.0f + (cameraSpeed * 1.4f);
    } else {
        //return 50.0f + (cameraSpeed * 1.0f);
        return 10000.0f;
    }
}

static void recomp_check_camera_jumps(void) {
    static Vec3f camVelocity = {0};

    Vec3f posDelta;
    vec3_sub(&sCamData->srt.transl, &sCamData->positionMirror, &posDelta);

    Vec3f posProjected;
    vec3_add_with_scale(&sCamData->positionMirror, &camVelocity, gUpdateRateF, &posProjected);

    f32 distToProjected = vec3_distance(&sCamData->srt.transl, &posProjected);
    f32 speed = vec3_length(&camVelocity);
    f32 threshold = recomp_get_camera_jump_threshold(speed);
    // diPrintf("dist %f\n", &distToProjected);
    // if (distToProjected > (threshold * 0.5f)) {
    //     recomp_printf("dist %f / %f  (%f)\n", distToProjected, threshold, speed);
    // }
    if (distToProjected > threshold) {
        //recomp_printf("dist %f / %f  (%f)\n", distToProjected, threshold, speed);
        camVelocity.x = 0.0f;
        camVelocity.y = 0.0f;
        camVelocity.z = 0.0f;
        recomp_set_skip_camera_interpolation(TRUE);
    } else {
        camVelocity.x = posDelta.x;
        camVelocity.y = posDelta.y;
        camVelocity.z = posDelta.z;
        recomp_set_skip_camera_interpolation(FALSE);
    }
}

RECOMP_PATCH void CamControl_tick(void) {
    Object* player;
    f32 tSpeed;
    u8 onTitleScreen;

    if (menu_get_current() == MENU_TITLE_SCREEN) {
        onTitleScreen = TRUE;
    } else {
        onTitleScreen = FALSE;
    }
    
    player = sCamData->player;
    if (player == NULL) {
        return;
    }
    
    func_80008D90(player->parent);
    CamControl_store_player_coords(player);
    CamControl_average_player_speed(sCamData, player);
    
    //Optionally override the player's position (restored at end of function)
    if (sCamData->setPlayerPosition) {
        player->srt.transl.x = sCamData->newPlayerPosition.x;
        player->srt.transl.y = sCamData->newPlayerPosition.y;
        player->srt.transl.z = sCamData->newPlayerPosition.z;
        player->globalPosition.x = sCamData->newPlayerPosition.x;
        player->globalPosition.y = sCamData->newPlayerPosition.y;
        player->globalPosition.z = sCamData->newPlayerPosition.z;
        sCamData->setPlayerPosition = FALSE;
    }
    
    //Transform the player's coordinates based on their parent Object
    if (player->parent != NULL) {
        transform_point_by_object(
            player->srt.transl.x, player->srt.transl.y, player->srt.transl.z, 
            &player->srt.transl.x, &player->srt.transl.y, &player->srt.transl.z, 
            player->parent
        );
        player->srt.yaw += player->parent->srt.yaw;
    }
    
    //Set up easing into a different camera DLL (if needed)
    if (sEaseSetupNeeded) {
        //If duration is less than 2 frames, skip lerp and arrive at goal immediately
        if (sEaseDuration > 1) {
            tSpeed = 1.0f / sEaseDuration;
            if ((tSpeed <= 0.0f) || (tSpeed > 1.0f)) {
                tSpeed = 1.0f;
            }
            sCamData->tValue = 1.0f;
            sCamData->tSpeed = tSpeed;
            sCamData->lerpFlags = sEaseFlags;
        } else {
            sCamData->tValue = 0.0f;
            sCamData->lerpFlags = CamControl_Ease_None;
        }
        
        //Store the current camera DLL's translation/rotation/fov as the ease startpoint
        if (sCamData->tValue == 1.0f) {
            sCamData->easeStartX = sCamData->srt.transl.x;
            sCamData->easeStartY = sCamData->srt.transl.y;
            sCamData->easeStartZ = sCamData->srt.transl.z;
            sCamData->easeStartYaw = sCamData->srt.yaw;
            sCamData->easeStartPitch = sCamData->srt.pitch;
            sCamData->easeStartRoll = sCamData->srt.roll;
            sCamData->goalFov = sCamData->fov;
        }
        
        sPreviousID = sActiveID;
        sPreviousFree = sActiveFree;
        sPreviousSetupVal = sActiveSetupVal;
        CamControl_replace_active_module(sNextID, sCamAction);
        sEaseSetupNeeded = FALSE;
        
        if (sCamAction != NULL) {
            mmFree(sCamAction);
            sCamAction = NULL;
        }
    }

    //Advance the camera
    if (sActiveModule != NULL) {
        sActiveModule->dll->vtbl->control(sCamData);
        CamControl_update_camera(sCamData);
    }

    //Update the LockIcon and related flags
    if (onTitleScreen == FALSE) {
        if (sCamData->target == NULL) {
            sCamData->highlight = CamControl_find_highlight_object(sCamData, player);
        }
        
        if (sActiveID != DLL_ID_ATTENTIONCAM1) {
            sCamData->targetFlags &= ~ARROW_FLAG_1_Interacted;
        }
    }

    // @recomp: Check for sudden camera movements
    recomp_check_camera_jumps();
    
    sCamData->positionMirror.x = sCamData->srt.transl.x;
    sCamData->positionMirror.y = sCamData->srt.transl.y;
    sCamData->positionMirror.z = sCamData->srt.transl.z;
    sCamData->highlightFlags = 0;

    //Restore the player's transl/globalPosition
    CamControl_restore_player_coords(player);
}

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
