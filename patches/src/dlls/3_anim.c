#include "patches.h"
#include "matrix_groups.h"

#include "game/objects/object.h"
#include "game/objects/object_id.h"
#include "sys/gfx/animseq.h"
#include "sys/camera.h"
#include "sys/segment_1460.h"
#include "macros.h"
#include "dll.h"

#include "recomp/dlls/engine/3_ANIM_recomp.h"

#define MAX_SEQSLOTS 45
#define MAX_ACTORS 16

enum AnimEventType {
    ANIM_EVT_SETDURATION = -1,
    ANIM_EVT_SETTIME = 0,
    ANIM_EVT_MOVEMODE = 1,
    ANIM_EVT_ANIM = 2,
    ANIM_EVT_OVERRIDE = 3,
    ANIM_EVT_VTXANIM = 4,
    ANIM_EVT_SOFTWARE = 5,
    ANIM_EVT_SFX = 6,
    ANIM_EVT_GROUND_MODE = 7,
    ANIM_EVT_TUNE = 8,
    ANIM_EVT_ANGLE_MODE = 9,
    ANIM_EVT_LOOK_AT = 10,
    ANIM_EVT_CODE = 11,
    ANIM_EVT_SPEECH = 12,
    ANIM_EVT_ENVFX = 13,
    ANIM_EVT_STORYBOARD = 14,
    ANIM_EVT_SFX_WITH_DURATION = 15
};

enum AnimEnvFxEventType {
    ANIM_EVT_ENVFX_SET_MUSIC = 0,
    ANIM_EVT_ENVFX_APPLY = 2,
    ANIM_EVT_ENVFX_PARTFX = 3,
    ANIM_EVT_ENVFX_4 = 4, // noop
    ANIM_EVT_ENVFX_PROJGFX = 5,
    ANIM_EVT_ENVFX_WARP = 6,
    ANIM_EVT_ENVFX_SFX = 7,
    ANIM_EVT_ENVFX_BLINK = 8,
    ANIM_EVT_ENVFX_SCREEN_FX = 9,
    ANIM_EVT_ENVFX_SUBTITLES = 10,
    ANIM_EVT_ENVFX_SET_BIT = 11,
    ANIM_EVT_ENVFX_CLEAR_BIT = 12,
    ANIM_EVT_ENVFX_CMDMENU_BUTTON_OVERRIDE = 13,
    ANIM_EVT_ENVFX_EYELID_R = 14,
    ANIM_EVT_ENVFX_EYELID_L = 15
};

enum AnimCurvesKeyframeChannels {
/*00*/ CHANNEL_headRotateZ = 0,
/*01*/ CHANNEL_headRotateX = 1,
/*02*/ CHANNEL_headRotateY = 2,
/*03*/ CHANNEL_opacity = 3,
/*04*/ CHANNEL_dayTime = 4,
/*05*/ CHANNEL_scale = 5,
/*06*/ CHANNEL_rotateZ = 6,
/*07*/ CHANNEL_rotateY = 7,
/*08*/ CHANNEL_rotateX = 8,
/*09*/ CHANNEL_animSpeed = 9,
/*0A*/ CHANNEL_animBlendSpeed = 10,
/*0B*/ CHANNEL_translateZ = 11,
/*0C*/ CHANNEL_translateY = 12,
/*0D*/ CHANNEL_translateX = 13,
/*0E*/ CHANNEL_fieldOfView = 14,
/*0F*/ CHANNEL_eyeX = 15,
/*10*/ CHANNEL_eyeY = 16,
/*11*/ CHANNEL_jaw = 17,
/*12*/ CHANNEL_soundVolume = 18
};

enum KeyframeInterpolationType {
    KF_INTERP_Bezier = 0,
    KF_INTERP_Linear = 1,
    KF_INTERP_Stepped = 2
};

typedef struct {
    Object* actor;
    s16 value;
    s8 type;
} QueuedEnvFx;

typedef struct {
    s32* events; // pointer to list of code events
    s16 numEvents;
    s16 time; // timestamp of code event
} CodeEventList;

/*0xC*/ extern f32 _data_C;
/*0x30*/ extern s8 _data_30;

/*0x24*/ extern f32 _bss_24;
/*0x28*/ extern f32 _bss_28;
/*0x2C*/ extern f32 _bss_2C;
/*0x30*/ extern s16 _bss_30;
/*0x32*/ extern s8 _bss_32;
/*0x38*/ extern QueuedEnvFx sEnvFxQueue[10];
/*0x88*/ extern s8 sEnvFxQueueCount;
/*0x89*/ extern s8 _bss_89;
/*0x8A*/ extern s8 _bss_8A;
/*0x8B*/ extern s8 _bss_8B;
/*0x8C*/ extern s32 sCameraModule;
/*0x90*/ extern s32 _bss_90;
/*0x94*/ extern s32 _bss_94;
/*0x98*/ extern s32 _bss_98;
/*0xA0*/ extern f32 _bss_A0;
/*0x198*/ extern s8 _bss_198[MAX_SEQSLOTS];
/*0x5F0*/ extern CodeEventList sCodeEvtQueue[20];
/*0x5A4*/ extern f32 _bss_5A4;
/*0x5A8*/ extern f32 _bss_5A8;
/*0x5B0*/ extern f32 _bss_5B0;
/*0x5C4*/ extern f32 _bss_5C4;
/*0x5C8*/ extern s32 _bss_5C8;
/*0x5D0*/ extern s32 _bss_5D0;
/*0x5D4*/ extern s32 _bss_5D4;
/*0x5DC*/ extern f32 _bss_5DC;
/*0x690*/ extern s32 sCodeEvtQueueCount;
/*0x6FC*/ extern Object *_bss_6FC;

extern void anim_func_72E0(Object* animObj);
extern s32 anim_func_8878(void);
extern f32 anim_channel_value(AnimObj_Data* st, s32 channel, s32 time);
extern Object* anim_func_2FE8(Object* animObj, AnimObj_Data* st, AnimObj_Setup* setup);
extern s8 anim_get_free_sfx_slot(AnimObj_Data* st);

_Bool recomp_isCameraInSeq = FALSE;

// Animation channels to check for stepped interpolation
static s32 recomp_channelsToCheck[] = {
    CHANNEL_translateZ,
    CHANNEL_translateY,
    CHANNEL_translateX,
    CHANNEL_rotateZ,
    CHANNEL_rotateY,
    CHANNEL_rotateX,
    CHANNEL_headRotateZ,
    CHANNEL_headRotateY,
    CHANNEL_headRotateX,
    CHANNEL_scale,
    CHANNEL_fieldOfView,
    // CHANNEL_animSpeed,
    // CHANNEL_animBlendSpeed,
};

static _Bool recomp_anim_stepped_interp_channel_check(Object* actor, AnimObj_Data* st, s32 channel, RecompObjInterpState* interpState) {
    // Determine the keyframe we're interpolating into
    s32 numKeyframes = st->channelTotalKeys[channel] & 0xFFF;
    AnimCurvesKeyframe* keyframes = &st->animCurvesKeyframes[st->channelFirstKeyIndex[channel]];

    s32 nextKf = 0;
    while ((nextKf < numKeyframes && keyframes[nextKf].timeOffset < st->time)) {
        nextKf += 1;
    }

    // Edge case: the game does not look at the next keyframe if we land on the first keyframe's time offset exactly
    if (nextKf < numKeyframes && keyframes[nextKf].timeOffset == st->time && nextKf != 0) {
        nextKf += 1;
    }

    _Bool stepped = FALSE;
    for (s32 kf = interpState->lastKeyframes[channel]; kf < nextKf; kf++) {
        s32 interpType = (keyframes[kf].interpolation & 3);
        s32 timeOffsetDiff = 0;
        f32 valueDiff = 0.0f;
        if (kf > 0) {
            // Look at how we got to this keyframe
            timeOffsetDiff = keyframes[kf].timeOffset - keyframes[kf - 1].timeOffset;
            valueDiff = keyframes[kf].value - keyframes[kf - 1].value;
            if (valueDiff < 0) valueDiff *= -1;
        }

        if (interpType >= KF_INTERP_Stepped && (valueDiff != 0.0f || kf == 0)) {
            // Hit stepped keyframe (skip if the value didn't change (i.e. two stepped keyframes in a row for the same val))
            stepped = TRUE;
        }
        
        if (timeOffsetDiff < 2 && valueDiff > 0.0f) {
            // Sharp value change in 1 frame, this is sometimes done instead of a stepped keyframe
            stepped = TRUE;
        }
    }

    interpState->lastKeyframes[channel] = nextKf;

    return stepped;
}

static void recomp_anim_stepped_interp_curves_check(Object* actor, AnimObj_Data* st, RecompObjInterpState *interpState) {
    // Check specific anim curve channels for stepped interpolation
    _Bool stepped = FALSE;
    for (s32 i = 0; i < ARRAYCOUNT_S(recomp_channelsToCheck); i++) {
        s32 channel = recomp_channelsToCheck[i];
        if (recomp_anim_stepped_interp_channel_check(actor, st, channel, interpState)) {
            stepped = TRUE;
        }
    }

    if (stepped) {
        // A channel did stepped interpolation, so also skip interp for frame interp
        if (actor->id == OBJ_AnimCamera) {
            recomp_skip_camera_interp();
        } else {
            recomp_obj_skip_interp(actor);
        }
    }
}

RECOMP_PATCH void anim_update_actor_transform(Object* animObj, Object* actor, AnimObj_Data* st) {
    f32 var_fa0;
    f32 var_fv0;
    f32 var_fv1;
    s16 temp_a1;
    s16 temp_a3;
    s16 var_v1;

    // @recomp: Check if interpolation should be skipped
    RecompObjInterpState *interpState = recomp_obj_get_interp_state(actor);
    _Bool recomp_checkInitialInterpSkip = FALSE;
    if (interpState == NULL) {
        const char *actorName = actor->def == NULL ? "<unknown>" : actor->def->name;
        recomp_eprintf("[anim_update_actor_transform] Failed to get obj interp state %s\n", actorName);
    } else {
        // Note: Don't check transform for AnimCamera, we handle this logic for the camera elsewhere
        recomp_checkInitialInterpSkip = interpState->lastSeqTime <= 0 && actor->id != OBJ_AnimCamera;
        recomp_anim_stepped_interp_curves_check(actor, st, interpState);
        interpState->lastSeqTime = st->time;
    }

    if ((actor->parent == animObj->parent) || (_bss_32 == 0)) {
        var_fv0 = animObj->srt.transl.x;
        var_fv1 = animObj->srt.transl.y;
        var_fa0 = animObj->srt.transl.z;
        var_v1 = animObj->srt.yaw;
    } else {
        var_fv0 = _bss_24;
        var_v1 = _bss_30;
        var_fv1 = _bss_28;
        var_fa0 = _bss_2C;
    }
    temp_a1 = animObj->srt.pitch;
    temp_a3 = animObj->srt.roll;
    if (actor != animObj) {
        if (st->unk7A & 1) {
            // @recomp: Save previous position
            Vec3f prevPos;
            prevPos.x = actor->srt.transl.x;
            prevPos.y = actor->srt.transl.y;
            prevPos.z = actor->srt.transl.z;
            if (st->unk62 == 2) {
                actor->srt.transl.x = (st->unk4C.x * st->unk58) + var_fv0;
                actor->srt.transl.y = (st->unk4C.y * st->unk58) + var_fv1;
                actor->srt.transl.z = (st->unk4C.z * st->unk58) + var_fa0;
            } else {
                actor->srt.transl.x = var_fv0;
                actor->srt.transl.y = var_fv1;
                actor->srt.transl.z = var_fa0;
            }
            // @recomp: For the first frame for this actor, check if the change in position
            //          is large enough that interpolation should be skipped.
            if (recomp_checkInitialInterpSkip) {
                f32 xDiff = actor->srt.transl.x - prevPos.x;
                f32 yDiff = actor->srt.transl.y - prevPos.y;
                f32 zDiff = actor->srt.transl.z - prevPos.z;
                if (xDiff < 0.0f) xDiff *= -1.0f;
                if (yDiff < 0.0f) yDiff *= -1.0f;
                if (zDiff < 0.0f) zDiff *= -1.0f;

                if (xDiff > 2.0f || yDiff > 2.0f || zDiff > 2.0f) {
                    recomp_obj_skip_interp(actor);
                }
            }
        }
        if (st->unk7A & 2) {
            // @recomp: Save previous rotation
            s16 prevRot[3];
            prevRot[0] = actor->srt.yaw;
            prevRot[1] = actor->srt.pitch;
            prevRot[2] = actor->srt.roll;
            if (st->unk62 == 2) {
                actor->srt.yaw = var_v1 + (s16) ((f32) st->yawDiff * st->unk58);
                actor->srt.pitch = temp_a1 + (s16) ((f32) st->pitchDiff * st->unk58);
                actor->srt.roll = temp_a3 + (s16) ((f32) st->rollDiff * st->unk58);
            } else {
                actor->srt.yaw = var_v1;
                actor->srt.pitch = temp_a1;
                actor->srt.roll = temp_a3;
            }
            // @recomp: For the first frame for this actor, check if the change in rotation
            //          is large enough that interpolation should be skipped.
            if (recomp_checkInitialInterpSkip) {
                s32 xDiff = actor->srt.yaw - prevRot[0];
                s32 yDiff = actor->srt.pitch - prevRot[1];
                s32 zDiff = actor->srt.roll - prevRot[2];
                if (xDiff < 0) xDiff *= -1;
                if (yDiff < 0) yDiff *= -1;
                if (zDiff < 0) zDiff *= -1;

                if (xDiff > 16 || yDiff > 16 || zDiff > 16) {
                    recomp_obj_skip_interp(actor);
                }
            }
        }
    }
    if ((st->unk87 != 0) && (st->unk84 != 0)) {
        anim_func_72E0(animObj);
    }
    get_object_child_position(actor, &actor->globalPosition.x, &actor->globalPosition.y, &actor->globalPosition.z);
}

// TODO: i dont trust this patch
// RECOMP_PATCH s32 anim_process_event(Object* animObj, ModelInstance* animObjModelInst, AnimCurvesEvent** events, s8 arg3, s32* arg4) {
//     AnimState* temp_v1;
//     f32 var_fv0;
//     f32 var_fv1;
//     ModelInstanceBlendshape *blendShape;
//     s32 var_v0;
//     s32 var_a0;
//     Object* actor;
//     s32 pad;
//     AnimObj_Data* st;
//     AnimObj_Setup* setup;
//     s8 var_t0;
//     s8 arg3_8;
//     s8 sp30;
//     s32 pad2;
//     AnimCurvesEvent* evt; // sp3C

//     evt = *events;
//     sp30 = arg3 & 1;
//     var_t0 = arg3 & 2;
//     arg3_8 = arg3 & 8;
//     if (sp30 == 0) {
//         var_t0 = 1;
//     }
//     st = animObj->data;
//     setup = (AnimObj_Setup*)animObj->setup;
//     actor = st->actor;
//     if (actor == NULL) {
//         actor = animObj;
//     }
//     switch (evt->type) {
//     case ANIM_EVT_ANIM:
//         if (arg3_8) { break; }
//         st->modAnimIdx = (s16) (evt->params & 0xFFF); // move
//         st->modAnimStartFrame = (u8) ((evt->params >> 8) & 0xF0); // startframe
//         if (animObjModelInst == NULL) {
//             break;
//         }
//         temp_v1 = animObjModelInst->animState0;
//         if (actor->curModAnimId == st->modAnimIdx) {
//             if (temp_v1->unk60[0] != 0) {
//                 var_v0 = 0;
//             } else {
//                 var_v0 = 1;
//             }
//         } else {
//             var_v0 = 1;
//         }
//         temp_v1 = animObjModelInst->animState0;
//         if ((var_t0 != 0) && (var_v0 != 0) && (animObjModelInst != NULL)) {
//             temp_v1->curAnimationFrame[0] = temp_v1->totalAnimationFrames[0] * actor->animProgress;
//             if (st->channelTotalKeys[CHANNEL_animBlendSpeed] != 0) {
//                 var_fv1 = anim_channel_value(st, CHANNEL_animBlendSpeed, st->time - 1);
//             } else {
//                 var_fv1 = 8.0f;
//             }
//             if (var_fv1 < 1.0f) {
//                 var_v0 = 1;
//                 // @recomp: Skip object interp on dramatic model animation changes
//                 // TODO: this isn't completely right... we should probably do this check in the objanim code
//                 //recomp_obj_skip_interp(actor);
//             } else {
//                 var_v0 = 0;
//             }
//             func_80023D30(actor, st->modAnimIdx, st->modAnimStartFrame * 0.00390625f, var_v0);
//             st->unk20 = 1.0f;
//         }
//         break;
//     case ANIM_EVT_MOVEMODE:
//         if (arg3_8) { break; }
//         if ((st->unk87 != 0) && (_bss_198[st->seqSlot] != 0)) {
//             st->unk84 = 0;
//         } else {
//             st->unk84 = 1 - st->unk84;
//         }
//         break;
//     case ANIM_EVT_GROUND_MODE:
//         st->unk86 = 1 - st->unk86;
//         break;
//     case ANIM_EVT_OVERRIDE:
//         if (arg3_8) { break; }

//         if (!(arg3 & 4)) {
//             actor = anim_func_2FE8(animObj, st, setup);
//             actor->curModAnimIdLayered = -1;
//         }
//         break;
//     case ANIM_EVT_CODE:
//         if (sCodeEvtQueueCount >= 20) {
//             // @recomp: Restore printf
//             recomp_eprintf("CODE OVERFLOW\n");
//         }
//         if ((var_t0 != 0) && (evt->params > 0) && (sCodeEvtQueueCount < 20)) {
//             sCodeEvtQueue[sCodeEvtQueueCount].events = (s32*)(evt + 1);
//             sCodeEvtQueue[sCodeEvtQueueCount].time = st->time;
//             sCodeEvtQueue[sCodeEvtQueueCount].numEvents = evt->params;
//             sCodeEvtQueueCount++;
//         }
//         st->eventIdx += evt->params;
//         break;
//     case ANIM_EVT_VTXANIM:
//         if ((arg3_8 == 0) && (var_t0 != 0) && (animObjModelInst != NULL)) {
//             if (animObjModelInst->model->blendshapes != NULL) {
//                 // (evt->params & 0xFF) == "move"
//                 var_fv0 = (evt->params >> 8) & 0xFF; // vel
//                 if (var_fv0 != 0.0f) {
//                     var_fv0 = 1.0f / var_fv0;
//                 } else {
//                     var_fv0 = 1.0f;
//                 }
//                 if ((animObjModelInst->model->unk71 & 1) && ((evt->params & 0xFF) < 0xF)) {
//                     blendShape = animObjModelInst->blendshapes;
//                     blendShape += 2;
//                     func_8001AF04(animObjModelInst, blendShape->id, (evt->params & 0xFF) - 1, var_fv0, 2, 0);
//                 } else {
//                     blendShape = animObjModelInst->blendshapes;
//                     func_8001AF04(animObjModelInst, blendShape->id, (evt->params & 0xFF) - 1, var_fv0, 0, 0);
//                 }
//             }
//         }
//         break;
//     case ANIM_EVT_STORYBOARD:
//         if (arg3_8) { break; }
//         gDLL_1_cmdmenu->vtbl->open_tutorial_textbox(evt->params, 160, 140);
//         break;
//     case ANIM_EVT_ENVFX:
//         if ((sp30 == 0) && (((evt->params >> 0xC) & 0xF) != ANIM_EVT_ENVFX_BLINK) && (sEnvFxQueueCount < 10)) {
//             sEnvFxQueue[sEnvFxQueueCount].actor = actor;
//             sEnvFxQueue[sEnvFxQueueCount].type = (evt->params >> 0xC) & 0xF;
//             if ((sEnvFxQueue[sEnvFxQueueCount].type == ANIM_EVT_ENVFX_SET_BIT) || (sEnvFxQueue[sEnvFxQueueCount].type == ANIM_EVT_ENVFX_CLEAR_BIT)) {
//                 // Gamebit IDs are 16-bit, so it's stored in the next event slot
//                 sEnvFxQueue[sEnvFxQueueCount].value = (evt + 1)->params;
//                 sEnvFxQueueCount += 1;
//             } else {
//                 sEnvFxQueue[sEnvFxQueueCount].value = evt->params & 0xFFF;
//                 sEnvFxQueueCount += 1;
//             }
//         }
//         break;
//     }
//     if (sp30 != 0) {
//         return 0;
//     }
//     if ((_bss_89 != 0) || (_bss_8A != 0)) {
//         if (evt->type == ANIM_EVT_ENVFX) {
//             switch ((evt->params >> 0xC) & 0xF) {
//             case ANIM_EVT_ENVFX_APPLY:
//                 func_80000860(actor, actor, evt->params & 0xFFF, 0);
//                 break;
//             case ANIM_EVT_ENVFX_WARP:
//                 warpPlayer(evt->params & 0xFFF, 0);
//                 break;
//             }
//         }
//         return 0;
//     }
//     switch (evt->type) {
//     case ANIM_EVT_GROUND_MODE:
//     case ANIM_EVT_TUNE:
//     case ANIM_EVT_ANGLE_MODE:
//     case ANIM_EVT_LOOK_AT:
//     case ANIM_EVT_CODE:
//     case ANIM_EVT_SPEECH:
//     case ANIM_EVT_STORYBOARD:
//         break;
//     case ANIM_EVT_SFX:
//         if (arg3_8) { break; }
//         if (((evt->params >> 0xC) & 0xF) != 0xF) {
//             gDLL_6_AMSFX->vtbl->play(animObj, 
//                                      ((evt->params & 0xFFF) + 1), 
//                                      ((((evt->params >> 0xC) & 0xF) * 7) + 0x16), 
//                                      NULL, 
//                                      NULL, 0, NULL);
//         } else {
//             if (gDLL_6_AMSFX->vtbl->is_playing(st->sfxHandles[3]) != 0) {
//                 gDLL_6_AMSFX->vtbl->stop(st->sfxHandles[3]);
//             }
//             st->sfxTimer[3] = 32000;
//             gDLL_6_AMSFX->vtbl->play(animObj, 
//                                      ((evt->params & 0xFFF) + 1), 
//                                      (s32) anim_channel_value(st, CHANNEL_soundVolume, st->time), 
//                                      &st->sfxHandles[3], 
//                                      NULL, 0, NULL);
//         }
//         break;
//     case ANIM_EVT_ENVFX:
//         switch ((evt->params >> 0xC) & 0xF) {
//         case ANIM_EVT_ENVFX_SET_MUSIC:
//             if (arg3_8) { break; }
//             gDLL_5_AMSEQ2->vtbl->set(animObj, (evt->params & 0xFFF) + 1, STUBBED_STR("anim.c"), 0, STUBBED_STR("(e->val&0xfff)+1"));
//             break;
//         case ANIM_EVT_ENVFX_APPLY:
//             func_80000860(actor, actor, evt->params & 0xFFF, 0);
//             break;
//         case ANIM_EVT_ENVFX_WARP:
//             if (arg3_8) { break; }
//             warpPlayer(evt->params & 0xFFF, 0);
//             break;
//         case ANIM_EVT_ENVFX_SFX:
//             if (arg3_8) { break; }
//             if (st->unk30 != 0) {
//                 gDLL_6_AMSFX->vtbl->stop(st->unk30);
//             }
//             st->unk30 = 0;
//             gDLL_6_AMSFX->vtbl->play(animObj, 
//                                      ((evt->params & 0xFFF) + 1), 
//                                      ((((evt->params >> 0xC) & 0xF) * 7) + 0x16), 
//                                      &st->unk30, NULL, 0, NULL);
//             break;
//         case ANIM_EVT_ENVFX_BLINK:
//             if (arg3_8) { break; }
//             st->blinkFrameR = evt->params;
//             st->blinkFrameL = st->blinkFrameR & 0xFFF;
//             break;
//         case ANIM_EVT_ENVFX_EYELID_R:
//             if (arg3_8) { break; }
//             st->blinkFrameR = evt->params & 0xFFF;
//             break;
//         case ANIM_EVT_ENVFX_EYELID_L:
//             if (arg3_8) { break; }
//             st->blinkFrameL = evt->params & 0xFFF;
//             break;
//         }
//         break;
//     case ANIM_EVT_SFX_WITH_DURATION:
//         if (arg3_8) { break; }
//         anim_get_free_sfx_slot(st);
//         if (((evt->params >> 0xC) & 0xF) != 0xF) {
//             gDLL_6_AMSFX->vtbl->play(animObj, 
//                                      ((evt->params & 0xFFF) + 1), 
//                                      ((((evt->params >> 0xC) & 0xF) * 7) + 0x16), 
//                                      &st->sfxHandles[st->sfxNextSlot], 
//                                      NULL, 0, NULL);
//             var_a0 = st->sfxNextSlot;
//             st->sfxNextSlot++;
//             if (st->sfxNextSlot >= 3) {
//                 st->sfxNextSlot = 0;
//             }
//         } else {
//             if (gDLL_6_AMSFX->vtbl->is_playing(st->sfxHandles[3]) != 0) {
//                 gDLL_6_AMSFX->vtbl->stop(st->sfxHandles[3]);
//             }
//             gDLL_6_AMSFX->vtbl->play(animObj, 
//                                      ((evt->params & 0xFFF) + 1), 
//                                      (s32) anim_channel_value(st, CHANNEL_soundVolume, st->time), 
//                                      &st->sfxHandles[3], NULL, 0, NULL);
//             var_a0 = 3;
//         }
//         evt->delay = (evt + 1)->delay;
//         (evt + 1)->type = 0x63;
//         st->sfxTimer[var_a0] = (evt + 1)->params;
//         break;
//     }
//     return 0;
// }

RECOMP_PATCH void anim_update_camera(void) {
    s32 _pad;
    AnimObj_Setup *animobjSetup;
    f32 sp184;
    f32 sp180;
    f32 sp17C;
    s16 sp17A;
    s16 sp178;
    s16 sp176;
    CamControl_Data* temp_v0;
    CamControl_Data sp54;
    Unk_DLL2_Func888 sp4C;
    Unk_DLL2_Func888 sp44;
    DLL_86_CamAction sp38;

    if (_bss_6FC != NULL) {
        if (anim_func_8878() != 0) {
            animobjSetup = (AnimObj_Setup*)_bss_6FC->setup;
            sp184 = _bss_6FC->globalPosition.x;
            sp180 = _bss_6FC->globalPosition.y;
            sp17C = _bss_6FC->globalPosition.z;
            sp17A = _bss_6FC->srt.yaw;
            sp178 = _bss_6FC->srt.pitch;
            sp176 = _bss_6FC->srt.roll;
            if (_bss_6FC->parent != NULL) {
                sp17A += _bss_6FC->parent->srt.yaw;
            }
            _bss_A0 = 1.0f;
            // @recomp: When the camera enters a seq, check if it's initial transform within the seq
            //          is different enough that we should skip interpolation.
            if (!recomp_isCameraInSeq) {
                temp_v0 = gDLL_2_Camera->vtbl->get_data();
                f32 xDiff = sp184 - temp_v0->srt.transl.x;
                f32 yDiff = sp180 - temp_v0->srt.transl.y;
                f32 zDiff = sp17C - temp_v0->srt.transl.z;
                if (xDiff < 0.0f) xDiff *= -1.0f;
                if (yDiff < 0.0f) yDiff *= -1.0f;
                if (zDiff < 0.0f) zDiff *= -1.0f;
                f32 yawDiff = (0x8000 - sp17A) - temp_v0->srt.yaw;
                f32 pitchDiff = -sp178 - temp_v0->srt.pitch;
                f32 rollDiff = sp176 - temp_v0->srt.roll;
                if (yawDiff < 0) yawDiff *= -1;
                if (pitchDiff < 0) pitchDiff *= -1;
                if (rollDiff < 0) rollDiff *= -1;

                if ((xDiff > 2.0f || yDiff > 2.0f || zDiff > 2.0f) || (yawDiff > 16 || pitchDiff > 16 || rollDiff > 16)) {
                    recomp_skip_camera_interp();
                }
                recomp_isCameraInSeq = TRUE;
            }
            if (_bss_8B == 0) {
                sp54.srt.transl.x = sp184;
                sp54.srt.transl.y = sp180;
                sp54.srt.transl.z = sp17C;
                sp54.srt.yaw = 0x8000 - sp17A;
                sp54.srt.pitch = -sp178;
                sp54.srt.roll = sp176;
                if (_data_30 != 0) {
                    sp54.fov = _bss_5DC;
                    _data_C = _bss_5DC;
                } else {
                    sp54.fov = _data_C;
                }
                gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAMTALK1, 0, 1, sizeof(sp54), &sp54, animobjSetup->unk24, 0xFF);
                _bss_8B = 1;
            } else {
                temp_v0 = gDLL_2_Camera->vtbl->get_data();
                temp_v0->srt.transl.x = sp184;
                temp_v0->srt.transl.y = sp180;
                temp_v0->srt.transl.z = sp17C;
                temp_v0->srt.yaw = 0x8000 - sp17A;
                temp_v0->srt.pitch = -sp178;
                temp_v0->srt.roll = sp176;
                if (_data_30 != 0) {
                    temp_v0->fov = _bss_5DC;
                    _data_C = _bss_5DC;
                } else {
                    temp_v0->fov = _data_C;
                }
                _bss_5A4 = temp_v0->srt.transl.x;
                _bss_5A8 = temp_v0->srt.transl.y;
                _bss_5B0 = temp_v0->srt.transl.z;
                _bss_5C8 = (s32) temp_v0->srt.yaw;
                _bss_5D0 = (s32) temp_v0->srt.pitch;
                _bss_5D4 = (s32) temp_v0->srt.roll;
                _bss_5C4 = temp_v0->fov;
            }
        }
    } else if (_bss_8B != 0) {
        // @recomp: Skip camera interp on cam module swap
        recomp_skip_camera_interp();
        switch (sCameraModule) {
        case DLL_ID_CAMSTATIC:
            sp4C.unk0 = _bss_90;
            sp4C.unk4 = (s8) _bss_94;
            gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAMSTATIC, 1, 3, sizeof(sp4C), &sp4C, _bss_98, 0xFF);
            //dummy_label_1: ; // @fake
            break;
        case DLL_ID_CAMLOCKON:
            sp44.unk0 = _bss_90;
            if (_bss_98 == 0) {
                sp44.unk4 = 1;
            }
            gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAMLOCKON, 1, 3, sizeof(sp44), &sp44, _bss_98, 0xFF);
            //dummy_label_2: ; // @fake
            break;
        case DLL_ID_CAMCLIMB:
            gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAMCLIMB, 1, 0, 0, NULL, _bss_98, 0xFF);
            break;
        case DLL_ID_CAMTALK1:
            sp54.srt.transl.x = _bss_5A4;
            sp54.srt.transl.y = _bss_5A8;
            sp54.srt.yaw = (s16) _bss_5C8;
            sp54.srt.pitch = (s16) _bss_5D0;
            sp54.srt.transl.z = _bss_5B0;
            sp54.srt.roll = (s16) _bss_5D4;
            sp54.fov = _bss_5C4;
            gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAMTALK1, 1, 0, sizeof(sp54), &sp54, 0, 0xFF);
            break;
        case DLL_ID_CAMSLIDE:
            gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAMSLIDE, 1, 0, 0, NULL, _bss_98, 0xFF);
            break;
        case DLL_ID_CAM1STPERSON:
            if (_bss_90 != 0) {
                sp38.unk0 = 90.0f;
                sp38.unk4 = 20.0f;
                sp38.unk8 = 5;
                gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAM1STPERSON, 1, 1, sizeof(sp38), &sp38, 0, 0xFF);
            } else {
                sp38.unk0 = 90.0f;
                sp38.unk4 = 20.0f;
                sp38.unk8 = 30;
                gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAM1STPERSON, 1, 0, sizeof(sp38), &sp38, 0, 0xFF);
            }
            //dummy_label_3: ; // @fake
            break;
        case DLL_ID_CAMSHIPBATTLE1:
            gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAMSHIPBATTLE1, 1, 0, _bss_90, &_bss_94, _bss_98, 0xFF);
            break;
        case DLL_ID_CAMDRAKOR:
            gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAMDRAKOR, 1, 0, 0, NULL, 0, 0xFF);
            break;
        default:
            gDLL_2_Camera->vtbl->change_camera_module(DLL_ID_CAMNORMAL, 0, _bss_90, 0, NULL, _bss_98, 0xFF);
            break;
        }
        sCameraModule = 0;
        _bss_8B = 0;
        _data_C = 60.0f;
    }
    // @recomp: Check if camera is no longer in a seq
    if (_bss_6FC == NULL) {
        recomp_isCameraInSeq = FALSE;
    }
    _data_30 = 0;
    _bss_6FC = NULL;
}
