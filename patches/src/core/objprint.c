#include "patches.h"
#include "matrix_groups.h"

#include "dlls/objects/common/group48.h"
#include "sys/camera.h"
#include "sys/math.h"
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
extern void func_80036058(Object*, Object*, ModelInstance*, Gfx**, Mtx**, Vertex**);

extern MtxF *D_800B2E10;
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

RECOMP_PATCH void objprint_func(Gfx** gdl, Mtx** mtxs, Vertex** vtxs, Triangle** tris, Object* obj, s8 visibility) {
    if (obj->stateFlags & OBJSTATE_DESTROYED) {
        return;
    }

    if (visibility == 0) {
        if (obj->objhitInfo != NULL && (obj->objhitInfo->unk5A & 0x30)) {
            obj->objhitInfo->unk9F = 2;
        }
    }

    if (obj->srt.flags & OBJFLAG_INVISIBLE) {
        return;
    }

    if (obj->parent != NULL && (obj->parent->srt.flags & OBJFLAG_INVISIBLE)) {
        return;
    }

    update_pi_manager_array(2, obj->id);
    dl_add_debug_info(*gdl, obj->id, "objects/objprint.c", 426);
    if (obj->dll != NULL) {
        if (!(obj->stateFlags & OBJSTATE_PRINT_DISABLED)) {
            // @recomp: Tag anything drawn from the print func not otherwise covered by other groups
            _Bool skipInterp;
            u32 objMtxGroup = recomp_obj_get_matrix_group(obj, &skipInterp);
            if (skipInterp) {
                gEXMatrixGroupSkipAll((*gdl)++, objMtxGroup + OBJ_PRINT_AUTO_MTX_GROUP_ID_START, 
                    G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            } else {
                gEXMatrixGroupSimpleNormalAuto((*gdl)++, objMtxGroup + OBJ_PRINT_AUTO_MTX_GROUP_ID_START, 
                    G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            }
            
            obj->dll->vtbl->print(obj, gdl, mtxs, vtxs, tris, visibility);
        } else if (visibility != 0) {
            draw_object(obj, gdl, mtxs, vtxs, tris, 1.0f);
        }
    } else if (visibility != 0) {
        draw_object(obj, gdl, mtxs, vtxs, tris, 1.0f);
    }
    if (obj->stateFlags & OBJSTATE_PENDING_MODEL_SWITCH) {
        obj_handle_model_switch(obj, obj->modelInsts[obj->modelInstIdx], obj->modelInsts[obj->modelInstIdx]->model);
    }
    if (obj->linkedObject != NULL && (obj->linkedObject->stateFlags & OBJSTATE_PENDING_MODEL_SWITCH)) {
        obj_handle_model_switch(obj->linkedObject, obj->linkedObject->modelInsts[obj->modelInstIdx], obj->linkedObject->modelInsts[obj->modelInstIdx]->model);
    }
    dl_add_debug_info(*gdl, (u32) -obj->id, "objects/objprint.c", 489);
    update_pi_manager_array(2, -1);
}

RECOMP_PATCH void draw_object(Object* obj, Gfx** gdl, Mtx** mtxs, Vertex** vtxs, Triangle** tris, f32 yPrescale) {
    s32 opacity;
    ModelInstance* modelInst;
    Model* model;
    SRT spDC;
    Object* parentObj;
    u8 blendR;
    u8 blendG;
    u8 blendB;
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
    opacity = obj->opacityWithFade;
    if (opacity > 0xFF) {
        opacity = 0xFF;
    }
    if (obj->def->flags & OBJDEF_SKY_LIT) {
        if (func_8001EBE0() != 0) {
            blendB = blendG = blendR = 0xFF;
        } else {
            blendR = sp6F;
            blendG = sp6E;
            blendB = sp6D;
        }
    } else {
        blendB = blendG = blendR = 0xFF;
    }
    if (obj->def->numAnimatedFrames > 0) {
        func_80036890(obj, 2);
    }
    if (BYTE_80091754 != 0) {
        var_v0 = (blendR * SHORT_800b2e14) >> 8;
        var_v1 = (blendG * SHORT_800b2e16) >> 8;
        var_a0 = (blendB * SHORT_800b2e18) >> 8;
        if (var_v0 > 0xFF) {
            var_v0 = 0xFF;
        }
        if (var_v1 > 0xFF) {
            var_v1 = 0xFF;
        }
        if (var_a0 > 0xFF) {
            var_a0 = 0xFF;
        }
        blendR = var_v0;
        blendG = var_v1;
        blendB = var_a0;
        BYTE_80091754 = 0;
    }
    if (BYTE_80091758 != 0) {
        blendR = BYTE_800b2e20;
        blendG = BYTE_800b2e21;
        blendB = BYTE_800b2e22;
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
                // @recomp: Factor parent matrix into joint model matrices
                if (recomp_objParentMtx != NULL) {
                    sp70 = recomp_model_instance_setup_absolute_matrices(modelInst, model->jointCount);
                } else {
                    sp70 = modelInst->matrices[modelInst->unk34 & 1];
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
                // @recomp: Factor parent matrix into joint model matrices
                if (recomp_objParentMtx != NULL) {
                    sp70 = recomp_model_instance_setup_absolute_matrices(modelInst, 1);
                }
            }
            modelInst->unk34 ^= 2;
            if ((obj->def->flags & OBJDEF_FLAG10) || (model->blendshapes != NULL)) {
                if (model->blendshapes != NULL) {
                    func_8001B100(modelInst);
                }
                if (obj->def->flags & OBJDEF_FLAG10) {
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
        } else {
            // @recomp: Extra else case to handle model instances that don't get their matrix list updated.
            //          We may still need to setup our copy here.
            if (recomp_objParentMtx != NULL) {
                s32 mtxCount = model->animCount != 0 ? model->jointCount : 1;
                sp70 = recomp_model_instance_setup_absolute_matrices(modelInst, mtxCount);
            } else {
                sp70 = modelInst->matrices[modelInst->unk34 & 1];
            }
        }
        if (model->unk71 & 4) {
            dl_set_env_color(&tempGdl, blendR, blendG, blendB, BYTE_800b2e23);
        }
        dl_set_prim_color(&tempGdl, blendR, blendG, blendB, opacity);
        if (!(obj->srt.flags & OBJFLAG_SKIP_MODEL_DL)) {
            // @recomp: Tag model draw
            if (skipInterp) {
                gEXMatrixGroupSkipAll(tempGdl++, 
                    (objMtxGroup * OBJ_MODEL_MTX_GROUP_MAX_MODELS) + obj->modelInstIdx + OBJ_MODEL_MTX_GROUP_ID_START, 
                    G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            } else {
                gEXMatrixGroupDecomposedNormal(tempGdl++, 
                    (objMtxGroup * OBJ_MODEL_MTX_GROUP_MAX_MODELS) + obj->modelInstIdx + OBJ_MODEL_MTX_GROUP_ID_START, 
                    G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            }
            // @recomp: Use matrix list selected above instead of only using the vanilla one
            gSPSegment(tempGdl++, SEGMENT_3, sp70);
            gSPSegment(tempGdl++, SEGMENT_5, modelInst->vertices[((s32) modelInst->unk34 >> 1) & 1]);
            if (opacity == 0xFF) {
                if (modelInst->unk34 & 0x10) {
                    load_model_display_list(model, modelInst);
                    modelInst->unk34 ^= 0x10;
                }
            } else {
                if (!(modelInst->unk34 & 0x10)) {
                    load_model_display_list2(model, modelInst);
                    modelInst->unk34 ^= 0x10;
                }
            }
            gSPDisplayList(tempGdl++, OS_PHYSICAL_TO_K0(modelInst->displayList));
            dl_set_all_dirty();
            tex_render_reset();
            // @recomp: Pop model tag
            gEXPopMatrixGroup(tempGdl++, G_MTX_MODELVIEW);
        }
        if (obj->linkedObject != NULL) {
            // @recomp: Tag linked object model draw
            if (skipInterp) {
                gEXMatrixGroupSkipAll(tempGdl++, 
                    (objMtxGroup * OBJ_MODEL_MTX_GROUP_MAX_MODELS) + obj->linkedObject->modelInstIdx + OBJ_LINKEDOBJ_MODEL_MTX_GROUP_ID_START, 
                    G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            } else {
                gEXMatrixGroupDecomposedNormal(tempGdl++, 
                    (objMtxGroup * OBJ_MODEL_MTX_GROUP_MAX_MODELS) + obj->linkedObject->modelInstIdx + OBJ_LINKEDOBJ_MODEL_MTX_GROUP_ID_START, 
                    G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            }
            // Draw linked object
            func_80035AF4(&tempGdl, &tempMtxs, &tempVtxs, &tempTris, obj, modelInst, &sp78, 0, 
                obj->linkedObject, obj->stateFlags & OBJSTATE_UNK_ATTACH_INDEX_MASK, (u8)opacity);
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

RECOMP_PATCH ModelInstance *func_80035AF4(Gfx** arg0, Mtx** arg1, Vertex** arg2, Triangle** arg3, Object* arg4, ModelInstance* arg5, MtxF* arg6, MtxF* arg7, Object* arg8, s32 arg9, s32 arg10) {
    MtxF* sp74;
    s32 sp70;
    ModelInstance* modelInst;
    MtxF* var_v1;
    Model* sp64;
    SRT sp4C;
    s32 i;

    D_800B2E10 = 0;

    if (arg8->def->numAnimatedFrames > 0) {
        func_80036890(arg8, 2);
    }
    modelInst = arg8->modelInsts[arg8->modelInstIdx];
    if (modelInst != NULL && !(arg5->unk34 & 8)) {
        sp64 = modelInst->model;
        if (arg4->def->numAttachPoints > 0) {
            sp70 = arg4->def->pAttachPoints[arg9].bones[arg4->modelInstIdx];
            sp4C.transl.f[0] = arg4->def->pAttachPoints[arg9].pos.f[0];
            sp4C.transl.f[1] = arg4->def->pAttachPoints[arg9].pos.f[1];
            sp4C.transl.f[2] = arg4->def->pAttachPoints[arg9].pos.f[2];
            sp4C.scale = 1.0f;
            sp4C.yaw = arg4->def->pAttachPoints[arg9].rot.s[0];
            sp4C.pitch = arg4->def->pAttachPoints[arg9].rot.s[1];
            sp4C.roll = arg4->def->pAttachPoints[arg9].rot.s[2];
            matrix_from_srt(arg6, &sp4C);
            matrix_concat_4x3(arg6, (MtxF*)&((f32*)arg5->matrices[arg5->unk34 & 1])[sp70 << 4], arg6);
        } else {
            // required to match
        }
        if (sp64->animCount != 0) {
            D_800B2E10 = arg6;
            func_80019730(modelInst, sp64, arg8, arg6);
            sp74 = modelInst->matrices[modelInst->unk34 & 1];
            func_80036058(arg8, arg4, modelInst, arg0, arg1, arg2);
            // @recomp: Factor parent matrix into joint model matrices
            if (recomp_objParentMtx != NULL) {
                sp74 = recomp_model_instance_setup_absolute_matrices(modelInst, sp64->jointCount);
            }
        } else {
            modelInst->unk34 ^= 1;
            arg7 = modelInst->matrices[modelInst->unk34 & 1];
            for (i = 0; i < 16; i++) {
                ((f32*)arg7->m)[i] = ((f32*)arg6->m)[i];
            }
            func_80036058(arg8, arg4, modelInst, arg0, arg1, arg2);
            add_matrix_to_pool(arg7, 1);
            D_800B2E10 = sp74 = arg7;
            // @recomp: Factor parent matrix into joint model matrices
            if (recomp_objParentMtx != NULL) {
                sp74 = recomp_model_instance_setup_absolute_matrices(modelInst, 1);
            }
        }
        modelInst->unk34 ^= 2;
        if ((arg8->def->flags & OBJDEF_FLAG10) || (sp64->blendshapes != NULL)) {
            if (sp64->blendshapes != NULL) {
                func_8001B100(modelInst);
            }
            func_8001DF60(arg8, modelInst);
        }
        if (sp64->envMapCount != 0) {
            func_8001F094(modelInst);
        }
        if (sp64->hitSphereCount != 0) {
            func_8001A8EC(modelInst, sp64, arg8, arg7, arg4);
        }
        if (!(arg4->srt.flags & OBJFLAG_SKIP_MODEL_DL)) {
            gSPSegment((*arg0)++, SEGMENT_3, sp74);
            gSPSegment((*arg0)++, SEGMENT_5, modelInst->vertices[(modelInst->unk34 >> 1) & 1]);
            if ((u8) arg10 == 0xFF) {
                if (modelInst->unk34 & 0x10) {
                    load_model_display_list(sp64, modelInst);
                    modelInst->unk34 ^= 0x10;
                }
            } else if (!(modelInst->unk34 & 0x10)) {
                load_model_display_list2(sp64, modelInst);
                modelInst->unk34 ^= 0x10;
            }
            gSPDisplayList((*arg0)++, OS_PHYSICAL_TO_K0(modelInst->displayList));
            dl_set_all_dirty();
            tex_render_reset();
            // @fake
            //if (D_800B2E10) {}
        }
        arg8->srt.transl.f[0] = D_800B2E10->m[3][0];
        arg8->srt.transl.f[1] = D_800B2E10->m[3][1];
        arg8->srt.transl.f[2] = D_800B2E10->m[3][2];
        if (arg8->parent != NULL) {
            transform_point_by_object(arg8->srt.transl.f[0], arg8->srt.transl.f[1], arg8->srt.transl.f[2], arg8->globalPosition.f, &arg8->globalPosition.f[1], &arg8->globalPosition.f[2], arg8->parent);
        } else {
            arg8->srt.transl.f[0] += gWorldX;
            arg8->srt.transl.f[2] += gWorldZ;
            arg8->globalPosition.f[0] = arg8->srt.transl.f[0];
            arg8->globalPosition.f[1] = arg8->srt.transl.f[1];
            arg8->globalPosition.f[2] = arg8->srt.transl.f[2];
        }
        if (arg8->def->numAttachPoints >= 2 && arg8->group == GROUP_UNK48) {
            if (arg8->parent != NULL) {
                camera_load_parent_projection(arg0);
            }
            ((DLL_IGROUP_48 *)arg8->dll)->vtbl->func10(arg8, arg0, arg1, arg2, arg3);
            if (arg8->parent != NULL) {
                setup_rsp_matrices_for_object(arg0, arg1, arg8->parent);
            }
        }
    }

    return modelInst;
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
