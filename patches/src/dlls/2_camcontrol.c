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
#include "dll.h"

#include "recomp/dlls/engine/2_camcontrol_recomp.h"

/*0x11C*/ extern Cam* sCam;
/*0x174*/ extern CamControl_Module* sActiveModule; //Active module: the camera module DLL currently in use
/*0x178*/ extern s32 sActiveID;          //Active module: DLL ID
/*0x17C*/ extern s32 sActiveLoadedIndex; //Active module: The loaded index of the camera module DLL currently in use
/*0x180*/ extern s32 sNextID;            //Ease (End Module): DLL ID of next module (when switching camera DLLs)
/*0x184*/ extern s32 sActiveFree;        //Active module: free setting
/*0x188*/ extern s32 sActiveSetupVal;    //Active Module: arg1 for DLL's setup function (TO-DO: figure this out)
/*0x18C*/ extern void* sCamData;         //Cam module specific data
/*0x190*/ extern u8 sCamSwitchNeeded;    //Boolean, starts a new camera ease
/*0x194*/ extern s32 sEaseDuration;      //Duration of easing between camera modules, in frames
/*0x198*/ extern u8 sEaseFlags;          //Which components to lerp during ease (x/y/z/yaw/pitch/roll)
/*0x19C*/ extern s32 sPreviousID;        //Ease (Start Module): DLL ID of previous module
/*0x1A0*/ extern s32 sPreviousFree;      //Ease (Start Module): free setting for previous module
/*0x1A4*/ extern s32 sPreviousSetupVal;  //Ease (Start Module): arg1 for previous module's setup function
/*0x1C0*/ extern f32 sFov;
/*0x1D0*/ extern Object* sLockIcon;
/*0x1D4*/ extern s16 sLetterboxHeight;  //Controls the letterboxing at the top/bottom of frame

extern void CamControl_update_camera(Cam* cam);
extern void CamControl_replace_active_module(u16 camDLLID, CameraAction* camAction);
extern void CamControl_store_player_coords(Object* arg0);
extern void CamControl_average_player_speed(Cam* cam, Object* arg1);
extern void CamControl_restore_player_coords(Object* obj);
extern Object* CamControl_find_highlight_object(Cam* cam, Object* arg1);

static Object* recomp_lastHighlightObj;

static f32 recomp_get_camera_jump_threshold(f32 cameraSpeed) {
    return 65.0f + (cameraSpeed * 1.0f);
}

static void recomp_check_camera_jumps(void) {
    static Vec3f camVelocity = {0};

    Vec3f posDelta;
    vec3_sub(&sCam->srt.transl, &sCam->positionMirror, &posDelta);

    Vec3f posProjected;
    vec3_add_with_scale(&sCam->positionMirror, &camVelocity, gUpdateRateF, &posProjected);

    f32 distToProjected = vec3_distance(&sCam->srt.transl, &posProjected);
    f32 speed = vec3_length(&camVelocity);
    f32 threshold = recomp_get_camera_jump_threshold(speed);
    // if (distToProjected > (threshold * 0.5f)) {
    //     recomp_printf("dist %f / %f  (%f)\n", distToProjected, threshold, speed);
    // }
    if (distToProjected > threshold) {
        camVelocity.x = 0.0f;
        camVelocity.y = 0.0f;
        camVelocity.z = 0.0f;
        recomp_skip_camera_interp();
        //recomp_printf("skip camera interp, big jump\n");
    } else {
        camVelocity.x = posDelta.x;
        camVelocity.y = posDelta.y;
        camVelocity.z = posDelta.z;
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
    
    player = sCam->player;
    if (player == NULL) {
        return;
    }
    
    func_80008D90(player->parent);
    CamControl_store_player_coords(player);
    CamControl_average_player_speed(sCam, player);
    
    //Optionally override the player's position (restored at end of function)
    if (sCam->setPlayerPosition) {
        player->srt.transl.x = sCam->newPlayerPosition.x;
        player->srt.transl.y = sCam->newPlayerPosition.y;
        player->srt.transl.z = sCam->newPlayerPosition.z;
        player->globalPosition.x = sCam->newPlayerPosition.x;
        player->globalPosition.y = sCam->newPlayerPosition.y;
        player->globalPosition.z = sCam->newPlayerPosition.z;
        sCam->setPlayerPosition = FALSE;
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
    if (sCamSwitchNeeded) {
        //If duration is less than 2 frames, skip lerp and arrive at goal immediately
        if (sEaseDuration > 1) {
            tSpeed = 1.0f / sEaseDuration;
            if ((tSpeed <= 0.0f) || (tSpeed > 1.0f)) {
                tSpeed = 1.0f;
            }
            sCam->tValue = 1.0f;
            sCam->tSpeed = tSpeed;
            sCam->easeFlags = sEaseFlags;
        } else {
            sCam->tValue = 0.0f;
            sCam->easeFlags = Cam_Ease_None;
        }
        
        //Store the current camera DLL's translation/rotation/fov as the ease startpoint
        if (sCam->tValue == 1.0f) {
            sCam->easeStartX = sCam->srt.transl.x;
            sCam->easeStartY = sCam->srt.transl.y;
            sCam->easeStartZ = sCam->srt.transl.z;
            sCam->easeStartYaw = sCam->srt.yaw;
            sCam->easeStartPitch = sCam->srt.pitch;
            sCam->easeStartRoll = sCam->srt.roll;
            sCam->goalFov = sCam->fov;
        }
        
        sPreviousID = sActiveID;
        sPreviousFree = sActiveFree;
        sPreviousSetupVal = sActiveSetupVal;
        CamControl_replace_active_module(sNextID, sCamData);
        sCamSwitchNeeded = FALSE;
        
        if (sCamData != NULL) {
            mmFree(sCamData);
            sCamData = NULL;
        }
    }

    //Advance the camera
    if (sActiveModule != NULL) {
        sActiveModule->dll->vtbl->control(sCam);
        CamControl_update_camera(sCam);
    }

    //Update the LockIcon and related flags
    if (onTitleScreen == FALSE) {
        if (sCam->target == NULL) {
            sCam->highlight = CamControl_find_highlight_object(sCam, player);
        }
        
        if (sActiveID != DLL_ID_ATTENTIONCAM) {
            sCam->targetFlags &= ~ARROW_FLAG_1_Interacted;
        }
    }

    // @recomp: Skip interp for lock icon if the target changes
    if (recomp_lastHighlightObj != sCam->highlight && sLockIcon != NULL) {
        if (sCam->highlight != NULL) {
            recomp_obj_skip_interp(sLockIcon);
        }
        recomp_lastHighlightObj = sCam->highlight;
    }

    // @recomp: Check for sudden camera movements
    if (!recomp_isCameraInSeq) {
        recomp_check_camera_jumps();
    }
    
    sCam->positionMirror.x = sCam->srt.transl.x;
    sCam->positionMirror.y = sCam->srt.transl.y;
    sCam->positionMirror.z = sCam->srt.transl.z;
    sCam->highlightFlags = 0;

    //Restore the player's transl/globalPosition
    CamControl_restore_player_coords(player);
}

RECOMP_PATCH void CamControl_update_camera(Cam* cam) {
    Camera* camera;
    f32 tValue;
    f32 spline[4];

    set_camera_selector(0);
    camera = get_main_camera();
    camera->srt.yaw = cam->srt.yaw;
    camera->srt.pitch = cam->srt.pitch;
    camera->srt.roll = cam->srt.roll;
    camera->srt.transl.x = cam->srt.transl.x;
    camera->srt.transl.y = cam->srt.transl.y;
    camera->srt.transl.z = cam->srt.transl.z;

    sFov = cam->fov;

    if (cam->tValue > 0.0f) {
        cam->tValue -= cam->tSpeed * gUpdateRateF;

        //Get Hermite-eased tValue for interpolation
        spline[3] = 0.0f;
        spline[2] = 0.0f;
        spline[0] = 0.0f;
        spline[1] = 1.0f;
        tValue = 1.0f - curves_hermite(spline, cam->tValue, 0);

        //Linear interpolation (position)
        if (cam->easeFlags & Cam_Ease_X) {
            camera->srt.transl.x = cam->easeStartX + (camera->srt.transl.x - cam->easeStartX)*tValue;
        }
        if (cam->easeFlags & Cam_Ease_Y) {
            camera->srt.transl.y = cam->easeStartY + (camera->srt.transl.y - cam->easeStartY)*tValue;
        }
        if (cam->easeFlags & Cam_Ease_Z) {
            camera->srt.transl.z = cam->easeStartZ + (camera->srt.transl.z - cam->easeStartZ)*tValue;
        }
        
        //Linear interpolation (rotation)
        if (cam->easeFlags & Cam_Ease_Yaw) {
            cam->dYaw = cam->easeStartYaw - (camera->srt.yaw & 0xFFFF);
            CIRCLE_WRAP(cam->dYaw)
            camera->srt.yaw = cam->easeStartYaw - ((s16)(cam->dYaw * tValue));
        }
        if (cam->easeFlags & Cam_Ease_Pitch) {
            cam->dPitch = cam->easeStartPitch - (camera->srt.pitch & 0xFFFF);
            CIRCLE_WRAP(cam->dPitch)
            camera->srt.pitch = cam->easeStartPitch - ((s16)(cam->dPitch * tValue));
        }
        if (cam->easeFlags & Cam_Ease_Roll) {
            cam->dRoll = cam->easeStartRoll - (camera->srt.roll & 0xFFFF);
            CIRCLE_WRAP(cam->dRoll)
            camera->srt.roll = cam->easeStartRoll - ((s16)(cam->dRoll * tValue));
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
    s32 letterboxGoal = cam->letterboxGoal == 0 ? 0 : cam->letterboxGoal + 6;
    if (sLetterboxHeight != letterboxGoal) {
        if (sLetterboxHeight < letterboxGoal) {
            sLetterboxHeight += cam->letterboxSpeed * (s32) gUpdateRateF;
            if (letterboxGoal < sLetterboxHeight) {
                sLetterboxHeight = letterboxGoal;
            }
        } else {
            sLetterboxHeight -= cam->letterboxSpeed * (s32) gUpdateRateF;
            if (sLetterboxHeight < letterboxGoal) {
                sLetterboxHeight = letterboxGoal;
            }
        }
        camera_set_letterbox(sLetterboxHeight);
    }
    
    cam->letterboxGoal = 0;
}

RECOMP_PATCH void CamControl_set_letterbox_goal(s32 height, s32 startAtGoal) {
    if (sCam->letterboxGoal < height) {
        sCam->letterboxGoal = height;
        sCam->letterboxSpeed = 1;
        
        if (startAtGoal) {
            // @recomp: Add back +6 offset removed from other scissor patches.
            //          Same patch as in CamControl_update_camera.
            camera_set_letterbox(height == 0 ? 0 : height + 6);
        }
    }
}
