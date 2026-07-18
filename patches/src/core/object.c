#include "patches.h"
#include "patches/main.h"
#include "recompdata.h"
#include "matrix_groups.h"

#include "PR/ultratypes.h"
#include "game/objects/object_id.h"
#include "sys/camera.h"
#include "sys/pi.h"
#include "sys/gfx/model.h"
#include "sys/asset.h"
#include "sys/dll.h"
#include "sys/exception.h"
#include "sys/linked_list.h"
#include "sys/memory.h"
#include "sys/objects.h"
#include "sys/objtype.h"
#include "sys/objhits.h"
#include "sys/newshadows.h"
#include "sys/objlib.h"
#include "macros.h"
#include "dll.h"

enum ObjFreeMode {
    OBJFREEMODE_FREE_ALL = 0,
    OBJFREEMODE_IMMEDIATE = 1, // unused
    OBJFREEMODE_DEFERRED = 2
};

extern s32 sObjFreeMode;

extern s16 sObjListVisibleStartIdx;
extern s16* D_800B18E4;
extern u32 D_800B18E8;
extern s32  *gFile_OBJECTS_TAB;
extern int gNumObjectsTabEntries;
extern void *gFile_TABLES_BIN;
extern s32  *gFile_TABLES_TAB;
extern int gNumTablesTabEntries;
extern ObjDef **gLoadedObjDefs;
extern u8 *gObjDefRefCount;
extern s16  *gFile_OBJINDEX;
extern int gObjIndexCount; //count of OBJINDEX.BIN entries
extern Object **gObjDeferredFreeList;
extern s32 gObjDeferredFreeListCount;
extern Object **sObjLockList;
extern s32 sObjLockListCount;
extern Object **gObjList; //global object list
extern s32 gNumObjs;
extern LinkedList gObjUpdateList;
extern s8 gEffectBoxCount;
extern Object *gEffectBoxes[20];
extern s32 D_800B1988;

extern void objClearAll(void);
extern void objMarkVisibilitySortDirty(void);
extern void objFreeObjdef(s32 tabIdx);
extern void objInitObject(Object *obj, ObjSetup *setup, s32 reset);

// 180 -> 500
#define RECOMP_MAX_OBJECTS 500

enum RecompObjInterpConfig {
    RECOMP_OBJINTERP_DISABLE = (1 << 0)
};

// Note: leave a little overhead for objects that are created but arent added to the world obj list
static Object* recomp_objMatrixGroupList[RECOMP_MAX_OBJECTS + 100];
static RecompObjInterpState recomp_objInterpStates[RECOMP_MAX_OBJECTS + 100];
static U32ValueHashmapHandle recomp_objMatrixGroupMap;
static u32 recomp_objMatrixGroupNext;

static void recomp_obj_interp_init(void) {
    recomp_objMatrixGroupMap = recomputil_create_u32_value_hashmap();
}

void recomp_obj_skip_interp(Object *obj) {
    u32 group;
    if (recomputil_u32_value_hashmap_get(recomp_objMatrixGroupMap, (collection_key_t)obj, &group)) {
        RecompObjInterpState *state = &recomp_objInterpStates[group];
        state->skipNextInterp = TRUE;
    } else {
        recomp_eprintf("[recomp_obj_skip_interp] Can't skip interp for object %s (%d), no matrix group was allocated!\n",
            obj->def == NULL ? "<null>" : obj->def->name, obj->id);
    }
}

static void recomp_obj_update_matrix_group_state(Object *obj, RecompObjInterpState *state) {
    // Only update once per tick
    if (state->lastGameTick == recomp_tickCounter) {
        return;
    }

    // Check if interp is always disabled for this object ID
    if (state->config & RECOMP_OBJINTERP_DISABLE) {
        state->skipInterp = TRUE;
        return;
    }

    // Determine if interpolation should be skipped for this frame
    _Bool snapTurned;
    if (state->lastYaw != obj->srt.yaw) {
        // TODO: doesnt work if parent has a non-zero yaw in a parent transition
        // If the object turned very sharply, interpolation will look wrong since the movement was too large.
        // This is mainly a problem with the player and the ability to instantly do a 180 in a single frame.
        f32 lastDir[2] = {
            mathCosfInterp(state->lastYaw),
            mathSinfInterp(state->lastYaw)
        };
        f32 dir[2] = {
            mathCosfInterp(obj->srt.yaw),
            mathSinfInterp(obj->srt.yaw)
        };

        // TODO: is this even right?
        f32 dot = (dir[0] * lastDir[0]) + (dir[1] * lastDir[1]);
        
        snapTurned = dot < 0;
    } else {
        snapTurned = FALSE;
    }

    state->skipInterp = recomp_skipAllInterp || state->skipNextInterp || snapTurned;

    if ((obj->seqSlot == SEQSLOT_NONE) && !(obj->stateFlags & OBJSTATE_IN_SEQ)) {
        for (s32 i = 0; i < 19; i++) {
            state->lastKeyframes[i] = 0;
            state->keyframeVelocities[i] = 0.0f;
        }
        state->lastSeqTime = 0;
    }

    // Update state
    state->lastGameTick = recomp_tickCounter;
    state->skipNextInterp = FALSE;
    state->lastYaw = obj->srt.yaw;
}

RecompObjInterpState* recomp_obj_get_interp_state(Object *obj) {
    u32 group;
    if (recomputil_u32_value_hashmap_get(recomp_objMatrixGroupMap, (collection_key_t)obj, &group)) {
        RecompObjInterpState *state = &recomp_objInterpStates[group];
        return state;
    }

    return NULL;
}

u32 recomp_obj_get_matrix_group(Object *obj, _Bool *skipInterpolation) {
    u32 group;
    if (recomputil_u32_value_hashmap_get(recomp_objMatrixGroupMap, (collection_key_t)obj, &group)) {
        RecompObjInterpState *state = &recomp_objInterpStates[group];
        recomp_obj_update_matrix_group_state(obj, state); // lazily update state
        if (skipInterpolation != NULL) {
            *skipInterpolation = state->skipInterp;
        }
        return group;
    }

    recomp_eprintf("recomp_obj_get_matrix_group lookup failed!\n");
    group = ARRAYCOUNT(recomp_objMatrixGroupList);
    if (skipInterpolation != NULL) {
        *skipInterpolation = FALSE;
    }
    return group;
}

static void recomp_obj_set_matrix_group_config(Object *obj, RecompObjInterpState *interpState) {
    u8 config = 0;
    
    switch (obj->id) {
        case OBJ_DFP_wallbar:
            config |= RECOMP_OBJINTERP_DISABLE; // object always teleports, don't try interp
            break;
    }

    interpState->config = config;
}

static u32 recomp_obj_alloc_matrix_group(Object *obj) {
    u32 group;
    if (recomputil_u32_value_hashmap_get(recomp_objMatrixGroupMap, (collection_key_t)obj, &group)) {
        return group;
    }

    for (u32 i = 0; i < ARRAYCOUNT(recomp_objMatrixGroupList); i++) {
        group = recomp_objMatrixGroupNext;

        // Try ascending group indices instead of filling in the smallest open slot to avoid letting
        // an object use the same group as another object that was just freed.
        recomp_objMatrixGroupNext++;
        if (recomp_objMatrixGroupNext == ARRAYCOUNT(recomp_objMatrixGroupList)) {
            recomp_objMatrixGroupNext = 0;
        }

        if (recomp_objMatrixGroupList[group] == NULL) {
            recomp_objMatrixGroupList[group] = obj;

            RecompObjInterpState *interpState = &recomp_objInterpStates[group];
            bzero(interpState, sizeof(RecompObjInterpState));
            recomp_obj_set_matrix_group_config(obj, interpState);

            recomputil_u32_value_hashmap_insert(recomp_objMatrixGroupMap, (collection_key_t)obj, group);
            return group;
        }
    }

    recomp_eprintf("Ran out of object matrix groups!\n");
    return ARRAYCOUNT(recomp_objMatrixGroupList) - 1;
}

static void recomp_obj_free_matrix_group(Object *obj) {
    u32 group;
    if (recomputil_u32_value_hashmap_get(recomp_objMatrixGroupMap, (collection_key_t)obj, &group)) {
        recomputil_u32_value_hashmap_erase(recomp_objMatrixGroupMap, (collection_key_t)obj);
        recomp_objMatrixGroupList[group] = NULL;
    }
}

RECOMP_PATCH void objInit(void) {
    int i;

    // @recomp: Init matrix group stuff
    recomp_obj_interp_init();

    //allocate some buffers
    // @recomp: Use increased max object count
    gObjDeferredFreeList = mmAlloc(sizeof(Object*) * RECOMP_MAX_OBJECTS, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:dellist"));
    sObjLockList = mmAlloc(sizeof(Object*) * 24, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:locklist"));
    D_800B18E4 = mmAlloc(0x10, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:contnobuf"));

    //load OBJINDEX.BIN and count number of entries
    assetRomLoad((void **) (&gFile_OBJINDEX), OBJINDEX_BIN);
    gObjIndexCount = (piRomGetFileSize(OBJINDEX_BIN) >> 1) - 1;
    while(!gFile_OBJINDEX[gObjIndexCount]) gObjIndexCount--;

    //load OBJECTS.TAB and count number of entries
    assetRomLoad((void **)&gFile_OBJECTS_TAB, OBJECTS_TAB);
    gNumObjectsTabEntries = 0;
    while(gFile_OBJECTS_TAB[gNumObjectsTabEntries] != -1) gNumObjectsTabEntries++;
    gNumObjectsTabEntries--;

    //init ref count and pointers
    gLoadedObjDefs = mmAlloc(gNumObjectsTabEntries * 4, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:deflist"));
    gObjDefRefCount = mmAlloc(gNumObjectsTabEntries, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:defno"));
    for(i = 0; i < gNumObjectsTabEntries; i++) gObjDefRefCount[i] = 0; //why not memset?

    //load TABLES.BIN and TABLES.TAB and count number of entries
    assetRomLoad((void **) (&gFile_TABLES_BIN), TABLES_BIN);
    assetRomLoad((void **) (&gFile_TABLES_TAB), TABLES_TAB);
    gNumTablesTabEntries = 0;
    while(gFile_TABLES_TAB[gNumTablesTabEntries] != -1) gNumTablesTabEntries++;

    //allocate global object list and some other buffers
    // @recomp: Use increased max object count
    gObjList = mmAlloc(sizeof(Object*) * RECOMP_MAX_OBJECTS, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:ObjList"));
    objHitInit();
    objClearAll();
}

RECOMP_PATCH void objAddObject(Object *obj, u32 initFlags) {
    if (obj->parent != NULL) {
        camTransformPointByObject(
            obj->srt.transl.x, obj->srt.transl.y, obj->srt.transl.z,
            &obj->globalPosition.x, &obj->globalPosition.y, &obj->globalPosition.z,
            obj->parent
        );
    } else {
        obj->globalPosition.x = obj->srt.transl.x;
        obj->globalPosition.y = obj->srt.transl.y;
        obj->globalPosition.z = obj->srt.transl.z;
    }

    obj->prevLocalPosition.x = obj->srt.transl.x;
    obj->prevLocalPosition.y = obj->srt.transl.y;
    obj->prevLocalPosition.z = obj->srt.transl.z;

    obj->prevGlobalPosition.x = obj->globalPosition.x;
    obj->prevGlobalPosition.y = obj->globalPosition.y;
    obj->prevGlobalPosition.z = obj->globalPosition.z;

    objInitObject(obj, obj->setup, FALSE);

    if (obj->objhitInfo != NULL) {
        obj->objhitInfo->unk10.x = obj->srt.transl.x;
        obj->objhitInfo->unk10.y = obj->srt.transl.y;
        obj->objhitInfo->unk10.z = obj->srt.transl.z;

        obj->objhitInfo->unk20.x = obj->srt.transl.x;
        obj->objhitInfo->unk20.y = obj->srt.transl.y;
        obj->objhitInfo->unk20.z = obj->srt.transl.z;
    }

    if (obj->def->mobileMapID > -1) {
        mapLoadMobileMap(obj->def->mobileMapID, obj);
    }

    update_pi_manager_array(0, -1);

    if (obj->def->flags & OBJDEF_IS_MOBILE_MAP) {
        objAddObjectType(obj, OBJTYPE_MobileMap);

        if (obj->updatePriority != OBJPRIORITY_MOBILE_MAP) {
            objSetPriority(obj, OBJPRIORITY_MOBILE_MAP);
        }
    } else {
        if (obj->updatePriority == 0) {
            objSetPriority(obj, OBJPRIORITY_DEFAULT);
        }
    }

    if (initFlags & OBJINIT_STANDALONE) {
        obj->stateFlags |= OBJSTATE_STANDALONE;
        gObjList[gNumObjs] = obj;
        gNumObjs += 1;

        /* default.dol
        if (gNumObjs > 349) {
            // "Failed assertion ObjListSize<MAX_OBJECTS"
        }
        */
        // @recomp: Restore print
        if (gNumObjs > RECOMP_MAX_OBJECTS) {
            recomp_eprintf("Failed assertion ObjListSize<MAX_OBJECTS\n");
        }

        objEnable(obj);
    }

    if (obj->def->unk5e >= 1) {
        objAddObjectType(obj, OBJTYPE_LookAt);
    }

    // Resorting by visibility isn't necessary if the object is visible since the object
    // was added to the end of the list where the visible objects are.
    if (obj->def->flags & OBJDEF_INVISIBLE) {
        objMarkVisibilitySortDirty();
    }

    if (obj->def->flags & OBJDEF_FLAG10) {
        objAddObjectType(obj, OBJTYPE_56);
    }

    write_c_file_label_pointers("objects/objects.c", 1143);

    // @recomp: Allocate matrix group for tagging
    recomp_obj_alloc_matrix_group(obj);
}

RECOMP_PATCH void objFreeObjectInternal(Object *obj, s32 onlySelf) {
    Object *obj2;
    /*sp+0xE4*/ LightAction lAction;
    AnimObj_Data *animObjdata;
    ModelInstance *modelInst;
    /*sp+0x40*/ Object *stackObjs[39]; // unknown exact length
    /*sp+0x3c*/ s32 k;
    /*sp+0x38*/ s32 numModels;
    /*sp+0x34*/ s32 i;
    /*sp+0x30*/ s32 numStackObjs;

    if (obj->dll != NULL) {
        update_pi_manager_array(4, obj->id);
        obj->dll->vtbl->Free(obj, onlySelf);
        update_pi_manager_array(4, -1);
        dllFree(obj->dll);
    }

    dll_amSfx->FreeObject(obj);
    gDLL_5_AMSEQ->vtbl->func17(obj);
    gDLL_13_Expgfx->vtbl->func9(obj);

    if (obj->def != NULL && (obj->def->flags & OBJDEF_FLAG10)) {
        objFreeObjectType(obj, OBJTYPE_56);
    }

    if (obj->def->flags & OBJDEF_IS_MOBILE_MAP) {
        objFreeObjectType(obj, OBJTYPE_MobileMap);

        if (!onlySelf) {
            numStackObjs = 0;

            for (i = 0; i < gNumObjs; i++) {
                obj2 = gObjList[i];

                if (obj == obj2->parent) {
                    obj2->parent = NULL;

                    if (obj2->setup != NULL) {
                        stackObjs[numStackObjs] = obj2;
                        numStackObjs++;
                        /* default.dol
                        if (39 < numStackObjs) {
                            // "world free obj list overflow\n"
                        }
                        */
                        // @recomp: Restore print
                        if (numStackObjs > ARRAYCOUNT_S(stackObjs)) {
                            recomp_eprintf("world free obj list overflow\n");
                        }
                    }
                }
            }

            for (i = 0; i < numStackObjs; i++) {
                objFreeObject(stackObjs[i]);
            }

            mapFree(obj->mobileMapID);
        }
    }

    if (!onlySelf && obj->controlNo == OBJCONTROL_AnimObj) {
        for (i = 0; i < gNumObjs; i++) {
            obj2 = gObjList[i];
            if (obj == obj2->animObj) {
                obj2->animObj = NULL;
            }
        }
    }

    for (k = 0; k < gNumObjs; k++) {
        obj2 = gObjList[k];
        if (obj2->controlNo == OBJCONTROL_AnimObj) {
            animObjdata = (AnimObj_Data*)obj2->data;
            if (obj == animObjdata->actor) {
                animObjdata->actor = NULL;
                animObjdata->unk9C = 1;
            }
        }
    }
    

    if (obj->def->unk5e >= 1) {
        objFreeObjectType(obj, OBJTYPE_LookAt);
    }

    if (obj->def->unk87 & 0x10) {
        lAction.unk12.asByte = 2;
        lAction.unke = 0;
        lAction.unk10 = obj->unkD6;
        lAction.unk1b = 0;

        gDLL_11_Newlfx->vtbl->func0(obj, obj, &lAction, 0, 0, 0);
    }

    if (obj->shadow != NULL) {
        if (obj->def->shadowType == OBJ_SHADOW_BOX) {
            shadows_func_8004D974(1);
        }

        if (obj->shadow->texture != NULL) {
            texFreeTexture(obj->shadow->texture);
        }

        if (obj->shadow->unk8 != NULL) {
            texFreeTexture(obj->shadow->unk8);
        }
    }

    if (obj->mesgQueue != NULL) {
        mmFree(obj->mesgQueue);
        obj->mesgQueue = NULL;
    }

    numModels = obj->def->numModels;
    for (k = 0; k < numModels; k++) {
        if (obj->modelInsts[k] != NULL) {
            modelInst = obj->modelInsts[k];
            modFreeModel(modelInst);
        }
    }

    objFreeObjdef(obj->tabIdx);

    if (obj->seqSlot >= 0) {
        if (!onlySelf) {
            gDLL_3_Animation->vtbl->end_obj_sequence(obj->seqSlot);
            obj->seqSlot = SEQSLOT_NONE;
        }
    }

    // @recomp: Free matrix group
    recomp_obj_free_matrix_group(obj);

    if (obj->srt.flags & OBJFLAG_OWNS_SETUP && obj->setup != NULL) {
        mmFree(obj->setup);
    }

    mmFree(obj);
}

RECOMP_PATCH void objFreeObject(Object *obj) {
    s32 i;
    s32 k;

    if (obj == NULL) {
        // "Failed assertion obj" (default.dol)
        // @recomp: Restore printf
        recomp_eprintf("objFreeObject: failed assertion obj\n");
        *((volatile s8*)NULL) = 0;
        return;
    }

    if (!(obj->stateFlags & OBJSTATE_DESTROYED)) {
        if (obj->unkD9 != 0) {
            objRemoveTouchCallbacksForObj(obj);
        }

        if (obj->stateFlags & OBJSTATE_STANDALONE) {
            for (i = 0; i < gNumObjs; i++) {
                if (obj == gObjList[i]) {
                    break;
                }
            }

            if (i < gNumObjs) {
                gNumObjs--;

                for (k = i; k < gNumObjs; k++) {
                    gObjList[k] = gObjList[k + 1];
                }
            }

            objDisable(obj);
            objMarkVisibilitySortDirty();
        }

        obj->stateFlags |= OBJSTATE_DESTROYED;

        if (obj->freeLock != 0) {
            for (i = 0; i < sObjLockListCount; i++) {
                if (obj == sObjLockList[i]) {
                    break;
                }
            }

            if (i == sObjLockListCount) {
                sObjLockList[sObjLockListCount] = obj;
                sObjLockListCount += 1;
            } else {
                // @recomp: Restore printf
                recomp_printf("objFreeTick %08x locked %d,already on list\n", obj, obj->freeLock);
            }
        } else if (sObjFreeMode == OBJFREEMODE_DEFERRED) {
            i = gObjDeferredFreeListCount;

            if (gObjDeferredFreeListCount != 0) {
                for (i = 0; i < gObjDeferredFreeListCount; i++) {
                    if (obj == gObjDeferredFreeList[i]) {
                        break;
                    }
                }
            }

            if (i == gObjDeferredFreeListCount) {
                gObjDeferredFreeList[gObjDeferredFreeListCount] = obj;
                gObjDeferredFreeListCount++;
                
                // @recomp: Use increased max object count
                if (gObjDeferredFreeListCount == RECOMP_MAX_OBJECTS) {
                    // @recomp: Restore printf
                    recomp_eprintf("objFreeObject: delete list size overrun\n");
                    gObjDeferredFreeListCount--;
                }
            }
        } else {
            objFreeObjectInternal(obj, /*onlySelf*/sObjFreeMode == OBJFREEMODE_FREE_ALL);
        }
    }
}
