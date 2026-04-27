#include "patches.h"
#include "recompdata.h"
#include "matrix_groups.h"

#include "PR/ultratypes.h"
#include "sys/camera.h"
#include "sys/fs.h"
#include "sys/gfx/model.h"
#include "sys/asset_thread.h"
#include "sys/dll.h"
#include "sys/exception.h"
#include "sys/linked_list.h"
#include "sys/memory.h"
#include "sys/objects.h"
#include "sys/objtype.h"
#include "sys/objhits.h"
#include "sys/newshadows.h"
#include "sys/main.h"
#include "macros.h"
#include "dll.h"

extern s16 D_800B18E0;
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
extern Object **D_800B1918;
extern s32 D_800B191C;
extern Object **gObjList; //global object list
extern s32 gNumObjs;
extern LinkedList gObjUpdateList;
extern s8 gEffectBoxCount;
extern Object *gEffectBoxes[20];
extern s32 D_800B1988;

extern void obj_clear_all(void);
extern void func_80020D90(void);
extern void copy_obj_position_mirrors(Object *obj, ObjSetup *setup, s32 param3);
extern void obj_free_objdef(s32 tabIdx);
extern void func_8002272C(Object *obj);

typedef struct {
    Object *lastParent;
    s16 lastYaw;
    u8 skipInterp;
} RecompObjMtxTagState;

static Object* recomp_objMatrixGroupList[1000];
static RecompObjMtxTagState recomp_objMtxTagStates[1000];
static U32ValueHashmapHandle recomp_objMatrixGroupMap;
static u32 recomp_objMatrixGroupNext;

// TODO: increase max objects

static void recomp_obj_update_matrix_group_state(Object *obj) {
    // Get state
    u32 group;
    if (!recomputil_u32_value_hashmap_get(recomp_objMatrixGroupMap, (collection_key_t)obj, &group)) {
        return;
    }

    RecompObjMtxTagState *state = &recomp_objMtxTagStates[group];

    // Determine if interpolation should be skipped for this frame
    _Bool parentChanged = state->lastParent != obj->parent;

    _Bool snapTurned;
    if (state->lastYaw != obj->srt.yaw) {
        // If the object turned very sharply, interpolation will look wrong since the movement was too large.
        // This is mainly a problem with the player and the ability to instantly do a 180 in a single frame.
        f32 lastDir[2] = {
            fcos16_precise(state->lastYaw),
            fsin16_precise(state->lastYaw)
        };
        f32 dir[2] = {
            fcos16_precise(obj->srt.yaw),
            fsin16_precise(obj->srt.yaw)
        };

        f32 dot = (dir[0] * lastDir[0]) + (dir[1] * lastDir[1]);
        
        snapTurned = dot < 0;
    } else {
        snapTurned = FALSE;
    }

    state->skipInterp = parentChanged || snapTurned;

    // Update state
    state->lastParent = obj->parent;
    state->lastYaw = obj->srt.yaw;
}

u32 recomp_obj_get_matrix_group(Object *obj, _Bool *skipInterpolation) {
    u32 group;
    if (recomputil_u32_value_hashmap_get(recomp_objMatrixGroupMap, (collection_key_t)obj, &group)) {
        if (skipInterpolation != NULL) {
            *skipInterpolation = recomp_objMtxTagStates[group].skipInterp;
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
            bzero(&recomp_objMtxTagStates[group], sizeof(RecompObjMtxTagState));
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

RECOMP_PATCH void init_objects(void) {
    int i;

    // @recomp: Init matrix group stuff
    recomp_objMatrixGroupMap = recomputil_create_u32_value_hashmap();

    //allocate some buffers
    gObjDeferredFreeList = mmAlloc(sizeof(Object*) * 180, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:dellist"));
    D_800B1918 = mmAlloc(sizeof(Object*) * 24, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:locklist"));
    D_800B18E4 = mmAlloc(0x10, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:contnobuf"));

    //load OBJINDEX.BIN and count number of entries
    queue_alloc_load_file((void **) (&gFile_OBJINDEX), OBJINDEX_BIN);
    gObjIndexCount = (get_file_size(OBJINDEX_BIN) >> 1) - 1;
    while(!gFile_OBJINDEX[gObjIndexCount]) gObjIndexCount--;

    //load OBJECTS.TAB and count number of entries
    queue_alloc_load_file((void **)&gFile_OBJECTS_TAB, OBJECTS_TAB);
    gNumObjectsTabEntries = 0;
    while(gFile_OBJECTS_TAB[gNumObjectsTabEntries] != -1) gNumObjectsTabEntries++;
    gNumObjectsTabEntries--;

    //init ref count and pointers
    gLoadedObjDefs = mmAlloc(gNumObjectsTabEntries * 4, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:deflist"));
    gObjDefRefCount = mmAlloc(gNumObjectsTabEntries, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:defno"));
    for(i = 0; i < gNumObjectsTabEntries; i++) gObjDefRefCount[i] = 0; //why not memset?

    //load TABLES.BIN and TABLES.TAB and count number of entries
    queue_alloc_load_file((void **) (&gFile_TABLES_BIN), TABLES_BIN);
    queue_alloc_load_file((void **) (&gFile_TABLES_TAB), TABLES_TAB);
    gNumTablesTabEntries = 0;
    while(gFile_TABLES_TAB[gNumTablesTabEntries] != -1) gNumTablesTabEntries++;

    //allocate global object list and some other buffers
    gObjList = mmAlloc(sizeof(Object*) * 180, ALLOC_TAG_OBJECTS_COL, ALLOC_NAME("obj:ObjList"));
    objhits_init();
    obj_clear_all();
}

RECOMP_PATCH void obj_add_object(Object *obj, u32 initFlags) {
    if (obj->parent != NULL) {
        transform_point_by_object(
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

    copy_obj_position_mirrors(obj, obj->setup, 0);

    if (obj->objhitInfo != NULL) {
        obj->objhitInfo->unk10.x = obj->srt.transl.x;
        obj->objhitInfo->unk10.y = obj->srt.transl.y;
        obj->objhitInfo->unk10.z = obj->srt.transl.z;

        obj->objhitInfo->unk20.x = obj->srt.transl.x;
        obj->objhitInfo->unk20.y = obj->srt.transl.y;
        obj->objhitInfo->unk20.z = obj->srt.transl.z;
    }

    if (obj->def->unka0 > -1) {
        func_80046320(obj->def->unka0, obj);
    }

    update_pi_manager_array(0, -1);

    if (obj->def->flags & OBJDATA_FLAG44_HasChildren) {
        obj_add_object_type(obj, OBJTYPE_7);

        if (obj->updatePriority != 90) {
            obj_set_update_priority(obj, 90);
        }
    } else {
        if (obj->updatePriority == 0) {
            obj_set_update_priority(obj, 80);
        }
    }

    if (initFlags & OBJ_INIT_FLAG1) {
        obj->unkB0 |= 0x10;
        gObjList[gNumObjs] = obj;
        gNumObjs += 1;

        /* default.dol
        if (gNumObjs > 349) {
            // "Failed assertion ObjListSize<MAX_OBJECTS"
        }
        */
        // @recomp: Restore print
        if (gNumObjs > 180) {
            recomp_eprintf("Failed assertion ObjListSize<MAX_OBJECTS\n");
        }

        obj_add_tick(obj);
    }

    if (obj->def->unk5e >= 1) {
        obj_add_object_type(obj, OBJTYPE_9);
    }

    if (obj->def->flags & OBJDATA_FLAG44_HaveModels) {
        func_80020D90();
    }

    if (obj->def->flags & OBJDATA_FLAG44_DifferentLightColor) {
        obj_add_object_type(obj, OBJTYPE_56);
    }

    write_c_file_label_pointers("objects/objects.c", 0x477);

    // @recomp: Allocate matrix group for tagging
    recomp_obj_alloc_matrix_group(obj);
}

RECOMP_PATCH void obj_free_object(Object *obj, s32 param2) {
    Object *obj2;
    /*sp+0xE4*/ LightAction lAction;
    ObjectAnim_Data *animObjdata;
    ModelInstance *modelInst;
    /*sp+0x40*/ Object *stackObjs[39]; // unknown exact length
    /*sp+0x3c*/ s32 k;
    /*sp+0x38*/ s32 numModels;
    /*sp+0x34*/ s32 i;
    /*sp+0x30*/ s32 numStackObjs;

    if (obj->dll != NULL) {
        update_pi_manager_array(4, obj->id);
        obj->dll->vtbl->free(obj, param2);
        update_pi_manager_array(4, -1);
        dll_unload(obj->dll);
    }

    gDLL_6_AMSFX->vtbl->func_1218(obj);
    gDLL_5_AMSEQ->vtbl->func17(obj);
    gDLL_13_Expgfx->vtbl->func9(obj);

    if (obj->def != NULL && obj->def->flags & 0x10) {
        obj_free_object_type(obj, 56);
    }

    if (obj->def->flags & 0x40) {
        obj_free_object_type(obj, 7);

        if (!param2) {
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
                        if (numStackObjs > (s32)ARRAYCOUNT(stackObjs)) {
                            recomp_eprintf("world free obj list overflow\n");
                        }
                    }
                }
            }

            for (i = 0; i < numStackObjs; i++) {
                obj_destroy_object(stackObjs[i]);
            }

            func_80045F48(obj->unk34);
        }
    }

    if (!param2 && obj->group == GROUP_UNK16) {
        for (i = 0; i < gNumObjs; i++) {
            obj2 = gObjList[i];
            if (obj == obj2->unkC0) {
                obj2->unkC0 = NULL;
            }
        }
    }

    for (k = 0; k < gNumObjs; k++) {
        obj2 = gObjList[k];
        if (obj2->group == GROUP_UNK16) {
            animObjdata = (ObjectAnim_Data*)obj2->data;
            if (obj == animObjdata->unk0) {
                animObjdata->unk0 = NULL;
                animObjdata->unk9C = 1;
            }
        }
    }
    

    if (obj->def->unk5e >= 1) {
        obj_free_object_type(obj, 9);
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
            tex_free(obj->shadow->texture);
        }

        if (obj->shadow->unk8 != NULL) {
            tex_free(obj->shadow->unk8);
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
            destroy_model_instance(modelInst);
        }
    }

    obj_free_objdef(obj->tabIdx);

    if (obj->unkB4 >= 0) {
        if (!param2) {
            gDLL_3_Animation->vtbl->func18((s32)obj->unkB4);
            obj->unkB4 = -1;
        }
    }

    // @recomp: Free matrix group
    recomp_obj_free_matrix_group(obj);

    if (obj->srt.flags & OBJFLAG_OWNS_SETUP && obj->setup != NULL) {
        mmFree(obj->setup);
    }

    mmFree(obj);
}

RECOMP_PATCH void update_objects(void) {
    void *node;
    Object *obj;
    Object *player;
    s32 nextFieldOffset;

    nextFieldOffset = gObjUpdateList.nextFieldOffset; // == 0x38, &obj->next

    func_80058FE8();

    update_obj_models();
    update_obj_hitboxes(gNumObjs);

    node = gObjUpdateList.head;

    for (obj = (Object*)node; node != NULL && obj->updatePriority == 100; obj = (Object*)node) {
        update_object(obj);
        node = *((void**)(nextFieldOffset + (u32)node));

        //if (obj->objhitInfo->unk58){} // fake match
    }

    for (obj = (Object*)node; node != NULL && obj->def->flags & 0x40; obj = (Object*)node) {
        update_object(obj);
        obj->matrixIdx = camera_alloc_object_matrix(obj);
        node = *((void**)(nextFieldOffset + (u32)node));
    }

    func_80025E58();

    while (node != NULL) {
        obj = (Object*)node;

        if (obj->objhitInfo != NULL) {
            if (obj->objhitInfo->unk5A != 8 || (obj->objhitInfo->unk58 & 1) == 0) {
                update_object(obj);
            }
        } else {
            update_object(obj);
        }

        node = *((void**)(nextFieldOffset + (u32)node));
    }

    player = get_player();
    if (player != NULL && player->linkedObject != NULL) {
        player->linkedObject->parent = player->parent;
        update_object(player->linkedObject);
    }

    obj_do_hit_detection(gNumObjs);

    node = gObjUpdateList.head;
    while (node != NULL) {
        obj = (Object*)node;
        func_8002272C(obj);
        node = *((void**)(nextFieldOffset + (u32)node));
    }

    player = get_player();
    if (player != NULL && player->linkedObject != NULL) {
        player->linkedObject->parent = player->parent;
        func_8002272C(player->linkedObject);
    }

    gDLL_24_Waterfx->vtbl->func_6E8(gUpdateRate);
    gDLL_15_Projgfx->vtbl->func2(gUpdateRate, 0);
    gDLL_14_Modgfx->vtbl->func2(0, 0, 0);
    gDLL_13_Expgfx->vtbl->func2(0, gUpdateRate, 0, 0);

    func_8002B6EC();

    gDLL_3_Animation->vtbl->func9();
    gDLL_3_Animation->vtbl->func5();
    gDLL_2_Camera->vtbl->tick(gUpdateRate);

    write_c_file_label_pointers("objects/objects.c", 0x169);

    // @recomp: Update matrix tagging state
    for (s32 i = 0; i < gNumObjs; i++) {
        recomp_obj_update_matrix_group_state(gObjList[i]);
    }
}
