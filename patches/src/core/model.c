#include "patches.h"

#include "PR/os.h"
#include "sys/gfx/model.h"
#include "sys/rarezip.h"
#include "sys/fs.h"
#include "sys/memory.h"

RECOMP_DECLARE_EVENT(recomp_on_model_load(s32 *id));
RECOMP_DECLARE_EVENT(recomp_on_model_loaded_rom(s32 id, Model *model));
RECOMP_DECLARE_EVENT(recomp_on_model_loaded(s32 id, Model *model));

RECOMP_DECLARE_EVENT(recomp_on_model_instance_loaded(s32 id, ModelInstance *modelInst));

#define MODEL_SLOT_ID(modelRef, idx) ((((s32*)modelRef) + (idx << 1)))[0]
#define MODEL_SLOT_MODEL(modelRef, idx) ((((s32*)modelRef) + (idx << 1)))[1]

extern s32* gFile_MODELS_TAB;
extern ModelSlot* gLoadedModels;
extern s32* gFreeModelSlots;
extern s32 gNumLoadedModels;
extern s32 gNumModelsTabEntries;
extern s32 gNumFreeModelSlots;
extern s16* gAuxBuffer;

extern void model_destroy(Model* model);

RECOMP_PATCH ModelInstance* model_load_create_instance(s32 id, u32 flags) {
    s32 i;
    s32 sp50;
    s32 sp4C;
    s32 sp48;
    s32 uncompressedSize;
    s16 sp42;
    s16 sp40;
    s16 sp3E;
    Model* model;
    ModelInstance* modelInst;
    s8 sp33;
    s8 isNewSlot;
    s8 isOldSlot;
    s32 temp;
    s32 sp28;

    if (id < 0) {
        id = -id;
    } else {
        read_file_region(MODELIND_BIN, gAuxBuffer, id * 2, 8);
        id = gAuxBuffer[0];
    }
    // @recomp: Invoke event (let mods change the ID)
    recomp_on_model_load(&id);
    for (i = 0; i < gNumLoadedModels; i++) {
        if (id != MODEL_SLOT_ID(gLoadedModels, i)) {
            continue;
        }

        model = (Model *)MODEL_SLOT_MODEL(gLoadedModels, i);
        modelInst = createModelInstance(model, flags, 0);
        if (modelInst != NULL) {
            model->refCount++;
            model_setup_anim_playback(modelInst, modelInst->animState0);
            if (modelInst->animState1 != NULL) {
                model_setup_anim_playback(modelInst, modelInst->animState1);
            }
        }
        // @recomp: Invoke event
        recomp_on_model_instance_loaded(id, modelInst);
        return modelInst;
    }
    if (id >= gNumModelsTabEntries) {
        id = 0;
    }
    isNewSlot = FALSE;
    isOldSlot = FALSE;
    if (gNumFreeModelSlots > 0) {
        gNumFreeModelSlots--;
        isNewSlot = TRUE;
        sp50 = gFreeModelSlots[gNumFreeModelSlots];
    } else {
        sp50 = gNumLoadedModels;
        isOldSlot = TRUE;
        gNumLoadedModels++;
    }
    sp4C = gFile_MODELS_TAB[id + 0];
    sp48 = gFile_MODELS_TAB[id + 1] - sp4C;
    read_file_region(MODELS_BIN, gAuxBuffer, sp4C, 0x10);
    sp42 = gAuxBuffer[0];
    sp3E = gAuxBuffer[2];
    sp40 = ((u16)mmAlign8(gAuxBuffer[1]) & 0xFFFF) + 0x90;
    uncompressedSize = rarezip_uncompress_size((u8*)gAuxBuffer + 8);
    // @recomp: Support uncompressed models
    _Bool isUncompressed = FALSE;
    if (uncompressedSize < 0) {
        isUncompressed = TRUE;
        uncompressedSize = (sp48 - 0xC);
    }
    sp28 = model_load_anim_remap_table(id, sp3E, sp42);
    sp28 += uncompressedSize + 500;
    model = mmAlloc(sp28, 9, NULL);
    if (model == NULL) {
        if (isNewSlot) {
            gNumFreeModelSlots++;
        }
        if (isOldSlot) {
            gNumLoadedModels--;
        }
        return NULL;
    }
    // @recomp: Support uncompressed models (note: uncompressed data starts at +0xC instead of +0xD)
    if (isUncompressed) {
        read_file_region(MODELS_BIN, model, sp4C + 0xC, uncompressedSize);
    } else {
        temp = (((u32)model + sp28) - sp48) - 0x10;
        modelInst = (ModelInstance *) (temp - (temp % 16));
        read_file_region(MODELS_BIN, (void*) modelInst, sp4C, sp48);
        rarezip_uncompress((u8*)modelInst + 8, (u8*)model, sp28);
    }
    // @recomp: Invoke event
    recomp_on_model_loaded_rom(id, model);
    model->materials = (ModelTexture*) ((u32)model->materials + (u32)model);
    model->vertices = (Vtx*) ((u32)model->vertices + (u32)model);
    model->faces = (ModelFacebatch*) ((u32)model->faces + (u32)model);
    model->hitSpheres = (HitSphere*) ((u32)model->hitSpheres + (u32)model);
    model->vertexGroups = (void*) ((u32)model->vertexGroups + (u32)model);
    model->vertexGroupOffsets = (void *) ((u32)model->vertexGroupOffsets + (u32)model);
    // @fake
    //if (model->textureAnimations) {}
    model->displayList = (Gfx *) ((u32)model->displayList + (u32)model);
    if (model->textureAnimations != NULL) {
        model->textureAnimations = (void*) ((u32)model->textureAnimations + (u32)model);
    }
    if (model->drawModes != NULL) {
        model->drawModes = (void*) ((u32)model->drawModes + (u32)model);
    }
    if (model->blendshapes != NULL) {
        model->blendshapes = (BlendshapeHeader*) ((u32)model->blendshapes + (u32)model);
    }
    model->anims = NULL;
    model->amap = NULL;
    if (model->joints != NULL) {
        model->joints = (ModelJoint*) ((u32)model->joints + (u32)model);
        if (model->collisionA != NULL) {
            model->collisionA = (f32*) ((u32)model->collisionA + (u32)model);
        }
        if (model->collisionB != NULL) {
            model->collisionB = (f32*) ((u32)model->collisionB + (u32)model);
        }
    } else {
        model->collisionA = NULL;
        model->collisionB = NULL;
    }
    if (model->edgeVectors != NULL) {
        model->edgeVectors = (void*) ((u32)model->edgeVectors + (u32)model);
    }
    if (model->facebatchBounds != NULL) {
        model->facebatchBounds = (void*) ((u32)model->facebatchBounds + (u32)model);
    }
    model->unk68 = sp40;
    model->modelId = id;
    model->refCount = 1;
    model->animCount = sp42;
    model->unk71 &= ~0x40;
    if (sp3E != 0) {
        model->unk71 |= 0x40;
    }
    sp33 = 0;
    for (i = 0; i < model->textureCount; i++) {
        model->materials[i].texture = tex_load(-((u32)model->materials[i].texture | 0x8000), 0);
        if (model->materials[i].texture == NULL) {
            sp33 = 1;
        }
    }
    if (sp33 == 0) {
        for (i = 0; i < model->unk70; i++) {
            if (model->faces[i].materialID != 0xFF && model->faces[i].materialID >= model->textureCount) {
                goto bail;
            }
        }
        patch_model_display_list_for_textures(model);
        uncompressedSize += (u32)model;
        uncompressedSize = mmAlign8(uncompressedSize);
        if (modanim_load(model, id, (u8*)uncompressedSize) == 0) {
            // @recomp: Invoke event
            recomp_on_model_loaded(id, model);
            modelInst = createModelInstance(model, flags, 1);
            if (modelInst != NULL) {
                model_setup_anim_playback(modelInst, modelInst->animState0);
                if (modelInst->animState1 != NULL) {
                    model_setup_anim_playback(modelInst, modelInst->animState1);
                }
                MODEL_SLOT_ID(gLoadedModels, sp50) = id;
                MODEL_SLOT_MODEL(gLoadedModels, sp50) = (s32)model;
                if (gNumLoadedModels < 70) {
                    // @recomp: Invoke event
                    recomp_on_model_instance_loaded(id, modelInst);
                    return modelInst;
                }
            }
        }
    }
bail:
    if (isOldSlot) {
        gNumLoadedModels--;
    }
    if (isNewSlot) {
        gNumFreeModelSlots++;
    }
    model_destroy(model);
    return NULL;
}
