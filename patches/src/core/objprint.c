#include "patches.h"
#include "matrix_groups.h"

#include "sys/camera.h"
#include "sys/rsp_segment.h"
#include "sys/map.h"
#include "sys/segment_1D900.h"
#include "sys/objprint.h"
#include "sys/objhits.h"
#include "sys/exception.h"
#include "sys/dl_debug.h"
#include "sys/objects.h"

extern void func_80036890(Object* arg0, s32 arg1);
extern void func_800357B4(Object*, ModelInstance*, Model*);

extern u8 BYTE_80091754;
extern u8 BYTE_80091758;
extern s16 SHORT_800b2e14;
extern s16 SHORT_800b2e16;
extern s16 SHORT_800b2e18;
extern MtxF *PTR_DAT_800b2e1c;
extern u8 BYTE_800b2e20;
extern u8 BYTE_800b2e21;
extern u8 BYTE_800b2e22;
extern u8 BYTE_800b2e23;

RECOMP_PATCH void objprint_func(Gfx** gdl, Mtx** mtxs, Vertex** vtxs, Triangle** tris, Object* arg4, s8 arg5) {
    if (arg4->unkB0 & 0x40) {
        return;
    }

    if (arg5 == 0) {
        if (arg4->objhitInfo != NULL && (arg4->objhitInfo->unk5A & 0x30)) {
            arg4->objhitInfo->unk9F = 2;
        }
    }

    if (arg4->srt.flags & OBJFLAG_INVISIBLE) {
        return;
    }

    if (arg4->parent != NULL && (arg4->parent->srt.flags & OBJFLAG_INVISIBLE)) {
        return;
    }

    update_pi_manager_array(2, arg4->id);
    dl_add_debug_info(*gdl, arg4->id, "objects/objprint.c", 0x1AAU);
    if (arg4->dll != NULL) {
        if (!(arg4->unkB0 & 0x4000)) {
            // @recomp: Tag anything drawn from the print func not otherwise covered by other groups
            _Bool skipInterp;
            u32 objMtxGroup = recomp_obj_get_matrix_group(arg4, &skipInterp);
            if (skipInterp) {
                gEXMatrixGroupSkipAll((*gdl)++, objMtxGroup + OBJ_PRINT_AUTO_MTX_GROUP_ID_START, 
                    G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            } else {
                gEXMatrixGroupSimpleNormalAuto((*gdl)++, objMtxGroup + OBJ_PRINT_AUTO_MTX_GROUP_ID_START, 
                    G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            }
            
            arg4->dll->vtbl->print(arg4, gdl, mtxs, vtxs, tris, arg5);
        } else if (arg5 != 0) {
            draw_object(arg4, gdl, mtxs, vtxs, tris, 1.0f);
        }
    } else if (arg5 != 0) {
        draw_object(arg4, gdl, mtxs, vtxs, tris, 1.0f);
    }
    if (arg4->unkB0 & 0x800) {
        func_80023A78(arg4, arg4->modelInsts[arg4->modelInstIdx], arg4->modelInsts[arg4->modelInstIdx]->model);
    }
    if (arg4->linkedObject != NULL && (arg4->linkedObject->unkB0 & 0x800)) {
        func_80023A78(arg4->linkedObject, arg4->linkedObject->modelInsts[arg4->modelInstIdx], arg4->linkedObject->modelInsts[arg4->modelInstIdx]->model);
    }
    dl_add_debug_info(*gdl, (u32) -arg4->id, "objects/objprint.c", 0x1E9U);
    update_pi_manager_array(2, -1);
}

RECOMP_PATCH void draw_object(Object* obj, Gfx** gdl, Mtx** mtxs, Vertex** vtxs, Triangle** tris, f32 yPrescale) {
    s32 spFC;
    ModelInstance* modelInst;
    Model* model;
    SRT spDC;
    Object* parentObj;
    u8 spD7;
    u8 spD6;
    u8 spD5;
    s32 var_v0;
    s32 var_v1;
    s32 var_a0;
    f32 spC4;
    f32 spC0;
    f32 spBC;
    s16 spBA;
    MtxF sp78;
    s32 pad;
    MtxF* sp70;
    u8 sp6F;
    u8 sp6E;
    u8 sp6D;
    Gfx* tempGdl;
    Mtx* tempMtxs;
    Vertex* tempVtxs;
    Triangle* tempTris;

    func_8001F81C(&sp6F, &sp6E, &sp6D);
    tempGdl = *gdl;
    tempMtxs = *mtxs;
    tempVtxs = *vtxs;
    tempTris = *tris;
    modelInst = obj->modelInsts[obj->modelInstIdx];
    if (modelInst == NULL) {
        return;
    }

    // @recomp: Get base matrix group
    _Bool skipInterp;
    u32 objMtxGroup = recomp_obj_get_matrix_group(obj, &skipInterp);

    model = modelInst->model;
    spFC = obj->opacityWithFade;
    if (spFC > 0xFF) {
        spFC = 0xFF;
    }
    if (obj->def->flags & 0x10000) {
        if (func_8001EBE0() != 0) {
            spD5 = spD6 = spD7 = 0xFF;
        } else {
            spD7 = sp6F;
            spD6 = sp6E;
            spD5 = sp6D;
        }
    } else {
        spD5 = spD6 = spD7 = 0xFF;
    }
    if (obj->def->numAnimatedFrames > 0) {
        func_80036890(obj, 2);
    }
    if (BYTE_80091754 != 0) {
        var_v0 = (spD7 * SHORT_800b2e14) >> 8;
        var_v1 = (spD6 * SHORT_800b2e16) >> 8;
        var_a0 = (spD5 * SHORT_800b2e18) >> 8;
        if (var_v0 > 0xFF) {
            var_v0 = 0xFF;
        }
        if (var_v1 > 0xFF) {
            var_v1 = 0xFF;
        }
        if (var_a0 > 0xFF) {
            var_a0 = 0xFF;
        }
        spD7 = var_v0;
        spD6 = var_v1;
        spD5 = var_a0;
        BYTE_80091754 = 0;
    }
    if (BYTE_80091758 != 0) {
        spD7 = BYTE_800b2e20;
        spD6 = BYTE_800b2e21;
        spD5 = BYTE_800b2e22;
        BYTE_80091758 = 0;
    } else {
        BYTE_800b2e23 = 0;
    }
    parentObj = obj->parent;
    if (parentObj != NULL) {
        setup_rsp_matrices_for_object(&tempGdl, &tempMtxs, parentObj);
        spC4 = obj->srt.transl.f[0];
        spC0 = obj->srt.transl.f[1];
        spBC = obj->srt.transl.f[2];
        spBA = obj->srt.yaw;
    }
    if ((obj->shadow != NULL) && (obj->shadow->gdl != NULL)) {
        if (obj->shadow->flags & OBJ_SHADOW_FLAG_CUSTOM_OBJ_POS) {
            spDC.transl.f[0] = obj->shadow->tr.f[0];
            spDC.transl.f[1] = obj->shadow->tr.f[1];
            spDC.transl.f[2] = obj->shadow->tr.f[2];
        } else {
            spDC.transl.f[0] = obj->srt.transl.f[0];
            spDC.transl.f[1] = obj->srt.transl.f[1];
            spDC.transl.f[2] = obj->srt.transl.f[2];
        }
        spDC.yaw = 0;
        spDC.roll = 0;
        spDC.pitch = 0;
        spDC.scale = 0.05f;
        if (parentObj != NULL && model->animCount != 0) {
            spDC.transl.f[0] += gWorldX;
            spDC.transl.f[2] += gWorldZ;
        }
        // @recomp: Tag shadow draw
        if (skipInterp) {
            gEXMatrixGroupSkipAll(tempGdl++, objMtxGroup + OBJ_SHADOW_MTX_GROUP_ID_START, 
                G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
        } else {
            gEXMatrixGroupSimpleNormal(tempGdl++, objMtxGroup + OBJ_SHADOW_MTX_GROUP_ID_START, 
                G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
        }
        camera_setup_object_srt_matrix(&tempGdl, &tempMtxs, &spDC, 1.0f, 0.0f, NULL);
        gSPDisplayList(tempGdl++, OS_PHYSICAL_TO_K0(obj->shadow->gdl));
        dl_set_all_dirty();
        tex_render_reset();
        // @recomp: Pop shadow tag
        gEXPopMatrixGroup(tempGdl++, G_MTX_MODELVIEW);
    }
    if (!(obj->srt.flags & OBJFLAG_SHADOW_ONLY)) {
        if (!(modelInst->unk34 & 8)) {
            if ((model->animCount != 0) && !(model->unk71 & 2)) {
                if (PTR_DAT_800b2e1c == 0) {
                    func_8001943C(obj, &sp78, yPrescale, 0.0f);
                    func_80019730(modelInst, model, obj, &sp78);
                } else {
                    func_80019730(modelInst, model, obj, PTR_DAT_800b2e1c);
                }
            } else {
                modelInst->unk34 ^= 1;
                sp70 = modelInst->matrices[modelInst->unk34 & 1];
                if (PTR_DAT_800b2e1c == 0) {
                    func_8001943C(obj, sp70, yPrescale, 0.0f);
                } else {
                    bcopy((void* ) PTR_DAT_800b2e1c, sp70, 0x40);
                }
                add_matrix_to_pool(sp70, 1);
            }
            modelInst->unk34 ^= 2;
            if ((obj->def->flags & 0x10) || (model->blendshapes != NULL)) {
                if (model->blendshapes != NULL) {
                    func_8001B100(modelInst);
                }
                if (obj->def->flags & 0x10) {
                    func_8001DF60(obj, modelInst);
                }
            }
            if (obj->def->numAnimatedFrames != 0) {
                func_8001E818(obj, model, modelInst);
            }
            if (model->envMapCount != 0) {
                func_8001F094(modelInst);
            }
            if (obj->unk74 != NULL) {
                func_80036438(obj);
            }
            if (model->hitSphereCount != 0) {
                func_8001A8EC(modelInst, model, obj, 0, obj);
            } else if ((obj->objhitInfo != NULL) && (obj->objhitInfo->unk5A & 0x20)) {
                if (obj->objhitInfo->unk9F != 0) {
                    obj->objhitInfo->unk9F--;
                }
            }
        }
        if (model->unk71 & 4) {
            dl_set_env_color(&tempGdl, spD7, spD6, spD5, BYTE_800b2e23);
        }
        dl_set_prim_color(&tempGdl, spD7, spD6, spD5, spFC);
        if (!(obj->srt.flags & OBJFLAG_SKIP_MODEL_DL)) {
            // @recomp: Tag model draw
            // TODO: ever more than 8 models?
            if (skipInterp) {
                gEXMatrixGroupSkipAll(tempGdl++, 
                    (objMtxGroup * 8) + obj->modelInstIdx + OBJ_MODEL_MTX_GROUP_ID_START, 
                    G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            } else {
                gEXMatrixGroupDecomposedNormal(tempGdl++, 
                    (objMtxGroup * 8) + obj->modelInstIdx + OBJ_MODEL_MTX_GROUP_ID_START, 
                    G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            }
            
            gSPSegment(tempGdl++, SEGMENT_3, modelInst->matrices[modelInst->unk34 & 1]);
            gSPSegment(tempGdl++, SEGMENT_5, modelInst->vertices[((s32) modelInst->unk34 >> 1) & 1]);
            if (spFC == 0xFF) {
                if (modelInst->unk34 & 0x10) {
                    load_model_display_list(model, modelInst);
                    modelInst->unk34 ^= 0x10;
                }
            } else if (!(modelInst->unk34 & 0x10)) {
                load_model_display_list2(model, modelInst);
                modelInst->unk34 ^= 0x10;
            }
            gSPDisplayList(tempGdl++, OS_PHYSICAL_TO_K0(modelInst->displayList));
            dl_set_all_dirty();
            tex_render_reset();
            // @recomp: Pop model tag
            gEXPopMatrixGroup(tempGdl++, G_MTX_MODELVIEW);
        }
        if (obj->linkedObject != NULL) {
            // @recomp: Tag linked object model draw
            // TODO: ever more than 8 models?
            if (skipInterp) {
                gEXMatrixGroupSkipAll(tempGdl++, 
                    (objMtxGroup * 8) + obj->linkedObject->modelInstIdx + OBJ_LINKEDOBJ_MODEL_MTX_GROUP_ID_START, 
                    G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            } else {
                gEXMatrixGroupDecomposedNormal(tempGdl++, 
                    (objMtxGroup * 8) + obj->linkedObject->modelInstIdx + OBJ_LINKEDOBJ_MODEL_MTX_GROUP_ID_START, 
                    G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            }
            // Draw linked object
            func_80035AF4(&tempGdl, &tempMtxs, &tempVtxs, &tempTris, obj, modelInst, &sp78, 0, obj->linkedObject, obj->unkB0 & 3, (u8)spFC);
            // @recomp: Pop model tag
            gEXPopMatrixGroup(tempGdl++, G_MTX_MODELVIEW);
        }
    }
    if ((obj->objhitInfo != NULL) && (obj->objhitInfo->unk5A & 0x20) && (modelInst->unk14 != NULL)) {
        func_800357B4(obj, modelInst, modelInst->model);
    }
    modelInst->unk34 |= 8;
    if (parentObj != NULL) {
        obj->srt.transl.f[0] = spC4;
        obj->srt.transl.f[1] = spC0;
        obj->srt.transl.f[2] = spBC;
        obj->srt.yaw = spBA;
        camera_load_parent_projection(&tempGdl);
    }
    *gdl = tempGdl;
    *mtxs = tempMtxs;
    *vtxs = tempVtxs;
    *tris = tempTris;
}

RECOMP_PATCH void func_800359D0(Object *obj, Gfx **gdl, Mtx **rspMtxs, Vertex **vtxs, Triangle **pols, u32 param_6)
{
    Gfx *mygdl;
    Mtx *outRspMtxs;
    void *d;
    u32 mainIdx;
    u32 otherIdx;
    ModelInstance *modelInst;
    ModelInstance *modelInst2;

    mygdl = *gdl;
    outRspMtxs = *rspMtxs;

    mainIdx = obj->modelInstIdx;
    if (param_6) {
        otherIdx = 0;
    } else {
        otherIdx = obj->def->numModels - 1;
    }
    
    // @recomp: Tag shadowtex model draw
    _Bool skipInterp;
    u32 objMtxGroup = recomp_obj_get_matrix_group(obj, &skipInterp);
    if (skipInterp) {
        gEXMatrixGroupSkipAll(mygdl++, objMtxGroup + OBJ_SHADOWTEX_MODEL_MTX_GROUP_ID_START, 
            G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    } else {
        gEXMatrixGroupDecomposedNormal(mygdl++, objMtxGroup + OBJ_SHADOWTEX_MODEL_MTX_GROUP_ID_START, 
            G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    }

    modelInst = obj->modelInsts[mainIdx];
    modelInst2 = obj->modelInsts[otherIdx];

    d = modelInst->matrices[modelInst->unk34 & 1];
    gSPSegment(mygdl++, SEGMENT_3, d);
    gSPSegment(mygdl++, SEGMENT_5, modelInst2->vertices[(modelInst2->unk34 >> 1) & 0x1]);
    gSPDisplayList(mygdl++, OS_K0_TO_PHYSICAL(modelInst2->displayList));

    dl_set_all_dirty();
    tex_render_reset();

    // @recomp: Pop tag
    gEXPopMatrixGroup(mygdl++, G_MTX_MODELVIEW);

    *gdl = mygdl;
    *rspMtxs = outRspMtxs;
}
