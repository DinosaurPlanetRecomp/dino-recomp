#include "reasset_models.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_iterator.h"
#include "reasset/files/reasset_textures.h"
#include "reasset/buffer.h"
#include "reasset/list.h"
#include "reasset/bin_ptr.h"

#include "PR/ultratypes.h"
#include "sys/gfx/model.h"
#include "sys/fs.h"
#include "sys/memory.h"
#include "sys/rarezip.h"

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    Buffer model;
    BinPtr modelPtr;
    Buffer amap;
    BinPtr amapPtr;
    Buffer modanims;
    BinPtr modanimsPtr;
} ModelEntry;

typedef struct {
    ReAssetID id;
    ReAssetID modelID;
    BinPtr binPtr;
} ModelIndexEntry;

static s32 modelOriginalCount;
static List modelList; // list[ModelEntry]
static U32List modelIDList; // list[ReAssetID]
static U32ValueHashmapHandle modelMap; // ReAssetID -> model list index
static ReAssetResolveMap modelResolveMap;
static ReAssetResolveMap modanimResolveMap;
static ReAssetResolveMap amapResolveMap;

static s32 modelIndexOriginalCount;
static List modelIndexList; // list[ModelIndexEntry]
static U32List modelIndexIDList; // list[ReAssetID]
static U32ValueHashmapHandle modelIndexMap; // ReAssetID -> model index list index
static ReAssetResolveMap modelIndexResolveMap;

static ModelIndexEntry* get_model_index(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(modelIndexMap, id, &listIdx)) {
        return list_get(&modelIndexList, listIdx);
    }

    return NULL;
}

static ModelIndexEntry* get_or_create_model_index(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(modelIndexMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < modelIndexOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base model index: %d", idData->identifier);
        }

        u32list_add(&modelIndexIDList, id);

        listIdx = list_get_length(&modelIndexList);
        
        ModelIndexEntry *entry = list_add(&modelIndexList);
        entry->id = id;

        recomputil_u32_value_hashmap_insert(modelIndexMap, id, listIdx);
    }

    return list_get(&modelIndexList, listIdx);
}

static ModelEntry* get_model(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(modelMap, id, &listIdx)) {
        return list_get(&modelList, listIdx);
    }

    return NULL;
}

static ModelEntry* get_or_create_model(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(modelMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < modelOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base model: %d", idData->identifier);
        }

        u32list_add(&modelIDList, id);

        listIdx = list_get_length(&modelList);
        
        ModelEntry *entry = list_add(&modelList);
        entry->id = id;
        entry->owner = idData->namespace;
        buffer_init(&entry->model, 0);
        buffer_init(&entry->modanims, 0);
        buffer_init(&entry->amap, 0);

        recomputil_u32_value_hashmap_insert(modelMap, id, listIdx);
    }

    return list_get(&modelList, listIdx);
}

static void model_list_element_free(void *element) {
    ModelEntry *entry = element;
    buffer_free(&entry->model);
    buffer_free(&entry->modanims);
    buffer_free(&entry->amap);
}

void reasset_models_init(void) {
    // Models
    modelOriginalCount = (reasset_fst_get_file_size(MODELS_TAB) / sizeof(s32)) - 2;
    s32 *originalModelTab = reasset_fst_alloc_load_file(MODELS_TAB, NULL);
    s16 *originalModanimTab = reasset_fst_alloc_load_file(MODANIM_TAB, NULL);
    s32 *originalAmapTab = reasset_fst_alloc_load_file(AMAP_TAB, NULL);

    list_init(&modelList, sizeof(ModelEntry), modelOriginalCount);
    list_set_element_free_callback(&modelList, model_list_element_free);
    u32list_init(&modelIDList, modelOriginalCount);
    modelMap = recomputil_create_u32_value_hashmap();
    modelResolveMap = reasset_resolve_map_create("Model");
    modanimResolveMap = reasset_resolve_map_create("ModelAnim");
    amapResolveMap = reasset_resolve_map_create("AMapEntry");

    // Add base models (preserving order)
    for (s32 i = 0; i < modelOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        ModelEntry *entry = get_or_create_model(id);

        s32 offset = originalModelTab[i];
        s32 size = originalModelTab[i + 1] - offset;

        buffer_set_base(&entry->model, MODELS_BIN, offset, size);

        offset = originalModanimTab[i];
        size = originalModanimTab[i + 1] - offset;

        buffer_set_base(&entry->modanims, MODANIM_BIN, offset, size);

        offset = originalAmapTab[i];
        size = originalAmapTab[i + 1] - offset;

        buffer_set_base(&entry->amap, AMAP_BIN, offset, size);
    }

    recomp_free(originalModelTab);
    recomp_free(originalModanimTab);
    recomp_free(originalAmapTab);

    // Model Indices
    modelIndexOriginalCount = (reasset_fst_get_file_size(MODELIND_BIN) / sizeof(s16)) - 1;
    s16 *originalIndexTab = reasset_fst_alloc_load_file(MODELIND_BIN, NULL);

    list_init(&modelIndexList, sizeof(ModelIndexEntry), modelIndexOriginalCount);
    u32list_init(&modelIndexIDList, modelIndexOriginalCount);
    modelIndexMap = recomputil_create_u32_value_hashmap();
    modelIndexResolveMap = reasset_resolve_map_create("ModelIndex");

    // Add base model indices (preserving order)
    for (s32 i = 0; i < modelIndexOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        ModelIndexEntry *entry = get_or_create_model_index(id);

        entry->modelID = reasset_base_id(originalIndexTab[i]);
    }

    recomp_free(originalIndexTab);
}

static void model_decompress(Buffer *buffer) {
    u32 size;
    void *data = buffer_get(buffer, &size);
    if (data == NULL || size < 0xD) {
        // Invalid model buffer
        return;
    }

    s32 decompressedSize = rarezip_uncompress_size((u8*)data + 8);
    if (decompressedSize <= 0) {
        // Already decompressed or zero size
        return;
    }

    // header + gzip header - 1 + data
    u32 newDataOffset = 8 + 4; // Note: Must be 4-byte aligned
    u32 newSize = newDataOffset + decompressedSize;
    void *newData = recomp_alloc(newSize);
    bzero(newData, newSize);

    bcopy(data, newData, 8); // header
    *((s32*)((u8*)newData + 0x8)) = -1; // decompressedSize
    rarezip_uncompress((u8*)data + 8, (u8*)newData + newDataOffset, decompressedSize);

    buffer_set(buffer, newData, newSize);

    recomp_free(newData);
}

static void reasset_models_repack_internal(void) {
    s32 newCount = list_get_length(&modelList);

    for (s32 i = 0; i < newCount; i++) {
        ModelEntry *entry = list_get(&modelList, i);

        if (entry->owner != REASSET_BASE_NAMESPACE) {
            // For any model that needs to be patched (those not owned by base), 
            // we need them to be uncompressed.
            model_decompress(&entry->model);
        }
    }

    // Calculate new sizes
    u32 newModelsBinSize = 0;
    u32 newModanimsBinSize = 0;
    u32 newAmapBinSize = 0;
    for (s32 i = 0; i < newCount; i++) {
        ModelEntry *entry = list_get(&modelList, i);
        
        newModelsBinSize += buffer_get_size(&entry->model);
        newModelsBinSize = mmAlign8(newModelsBinSize);

        newModanimsBinSize += buffer_get_size(&entry->modanims);
        newModanimsBinSize = mmAlign2(newModanimsBinSize);

        newAmapBinSize += buffer_get_size(&entry->amap);
        newAmapBinSize = mmAlign8(newAmapBinSize);
    }
    u32 newModelsTabSize = (newCount + 2) * sizeof(s32);
    u32 newModanimsTabSize = (newCount + 1) * sizeof(s16);
    u32 newAmapTabSize = (newCount + 1) * sizeof(s32);

    // Alloc new files
    s32 *newModelsTab = recomp_alloc(newModelsTabSize);
    void *newModelsBin = recomp_alloc(newModelsBinSize);
    s16 *newModanimsTab = recomp_alloc(newModanimsTabSize);
    void *newModanimsBin = recomp_alloc(newModanimsBinSize);
    s32 *newAmapTab = recomp_alloc(newAmapTabSize);
    void *newAmapBin = recomp_alloc(newAmapBinSize);

    bzero(newModelsBin, newModelsBinSize);
    bzero(newModanimsBin, newModanimsBinSize);
    bzero(newAmapBin, newAmapBinSize);

    // Rebuild
    s32 modelsBinOffset = 0;
    s32 modanimsBinOffset = 0;
    s32 amapBinOffset = 0;
    for (s32 i = 0; i < newCount; i++) {
        ModelEntry *entry = list_get(&modelList, i);
        ReAssetIDData *idData = reasset_id_lookup_data(entry->id);

        if (idData->namespace != REASSET_BASE_NAMESPACE) {
            const char *namespaceName;
            reasset_namespace_lookup_name(idData->namespace, &namespaceName);
            reasset_log("[reasset] New model: %s:%d\n", 
                namespaceName, idData->identifier);
        }

        reasset_resolve_map_resolve_id(modelResolveMap, entry->id, entry->owner, i);
        reasset_resolve_map_resolve_id(modanimResolveMap, entry->id, entry->owner, i);
        reasset_resolve_map_resolve_id(amapResolveMap, entry->id, entry->owner, i);

        newModelsTab[i] = modelsBinOffset;
        bin_ptr_set(&entry->modelPtr, newModelsBin, modelsBinOffset, buffer_get_size(&entry->model));
        buffer_copy_to_bin_ptr(&entry->model, &entry->modelPtr);
        modelsBinOffset += buffer_get_size(&entry->model);
        modelsBinOffset = mmAlign8(modelsBinOffset);

        newModanimsTab[i] = modanimsBinOffset;
        bin_ptr_set(&entry->modanimsPtr, newModanimsBin, modanimsBinOffset, buffer_get_size(&entry->modanims));
        buffer_copy_to_bin_ptr(&entry->modanims, &entry->modanimsPtr);
        modanimsBinOffset += buffer_get_size(&entry->modanims);
        modanimsBinOffset = mmAlign2(modanimsBinOffset);

        newAmapTab[i] = amapBinOffset;
        bin_ptr_set(&entry->amapPtr, newAmapBin, amapBinOffset, buffer_get_size(&entry->amap));
        buffer_copy_to_bin_ptr(&entry->amap, &entry->amapPtr);
        amapBinOffset += buffer_get_size(&entry->amap);
        amapBinOffset = mmAlign8(amapBinOffset);
    }

    newModelsTab[newCount] = modelsBinOffset;
    newModelsTab[newCount + 1] = -1;
    newModanimsTab[newCount] = modanimsBinOffset;
    newAmapTab[newCount] = amapBinOffset;

    reasset_assert((u32)modelsBinOffset <= newModelsBinSize, 
        "[reasset] Overflow writing MODELS.bin. %d > %d", modelsBinOffset, newModelsBinSize);
    reasset_assert((u32)modanimsBinOffset <= newModanimsBinSize, 
        "[reasset] Overflow writing MODANIM.bin. %d > %d", modanimsBinOffset, newModanimsBinSize);
    reasset_assert((u32)amapBinOffset <= newAmapBinSize, 
        "[reasset] Overflow writing AMAP.bin. %d > %d", amapBinOffset, newAmapBinSize);

    // Finalize resolve maps
    reasset_resolve_map_finalize(modelResolveMap);
    reasset_resolve_map_finalize(modanimResolveMap);
    reasset_resolve_map_finalize(amapResolveMap);

    // Set new files
    reasset_fst_set_internal(MODELS_TAB, newModelsTab, newModelsTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(MODELS_BIN, newModelsBin, newModelsBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt MODELS.tab & MODELS.bin (count: %d, bin size: 0x%X).\n", newCount, newModelsBinSize);
    reasset_fst_set_internal(MODANIM_TAB, newModanimsTab, newModanimsTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(MODANIM_BIN, newModanimsBin, newModanimsBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt MODANIM.tab & MODANIM.bin (count: %d, bin size: 0x%X).\n", newCount, newModanimsBinSize);
    reasset_fst_set_internal(AMAP_TAB, newAmapTab, newAmapTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(AMAP_BIN, newAmapBin, newAmapBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt AMAP.tab & AMAP.bin (count: %d, bin size: 0x%X).\n", newCount, newAmapBinSize);
}

static void reasset_models_indices_repack_internal(void) {
    s32 newCount = list_get_length(&modelIndexList);

    // Calculate new MODELIND.bin size
    u32 newIndexBinSize = (newCount + 1) * sizeof(s16);

    // Alloc new MODELIND.bin
    s16 *newBin = recomp_alloc(newIndexBinSize);
    bzero(newBin, newIndexBinSize);

    // Rebuild
    for (s32 i = 0; i < newCount; i++) {
        ModelIndexEntry *entry = list_get(&modelIndexList, i);
        ReAssetIDData *idData = reasset_id_lookup_data(entry->id);

        if (idData->namespace != REASSET_BASE_NAMESPACE) {
            const char *namespaceName;
            reasset_namespace_lookup_name(idData->namespace, &namespaceName);
            reasset_log("[reasset] New model index: %s:%d\n", 
                namespaceName, idData->identifier);
        }

        reasset_resolve_map_resolve_id(modelIndexResolveMap, entry->id, -1, i);

        bin_ptr_set(&entry->binPtr, newBin, i * sizeof(s16), sizeof(s16));

        // Note: Nothing to write to bin here. Values will be filled in during the patch stage
    }

    // Finalize resolve map
    reasset_resolve_map_finalize(modelIndexResolveMap);

    // Set new files
    reasset_fst_set_internal(MODELIND_BIN, newBin, newIndexBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt MODELIND.bin (count: %d, bin size: 0x%X).\n", newCount, newIndexBinSize);
}

void reasset_models_repack(void) {
    reasset_models_repack_internal();
    reasset_models_indices_repack_internal();
}

void reasset_models_patch(void) {
    ReAssetResolveMap tex1ResolveMap = reasset_textures_get_resolve_map(TEX1);

    // Patch in resolved IDs
    s32 numModels = list_get_length(&modelList);
    for (s32 i = 0; i < numModels; i++) {
        ModelEntry *entry = list_get(&modelList, i);
        if (entry->owner == REASSET_BASE_NAMESPACE) {
            continue;
        }

        const char *namespaceName;
        s32 identifier;
        reasset_id_lookup_name(entry->id, &namespaceName, &identifier);

        void *modelBin = entry->modelPtr.ptr;
        if (modelBin != NULL) {
            reasset_assert(rarezip_uncompress_size((u8*)modelBin + 8) == -1,
                "[reasset] bug! Model needs to be uncompressed for the patch stage.");
            
            Model *model = (Model*)((u8*)modelBin + 0xC);

            // Patch texture IDs
            ModelTexture *materials = (ModelTexture*)((u32)model->materials + (u32)model);
            for (s32 k = 0; k < model->textureCount; k++) {
                s32 *texIDPtr = (s32*)&materials[k].texture;
                s32 texID = *texIDPtr;

                if (!reasset_textures_is_base_id(TEX1, texID)) {
                    s32 resolvedID = reasset_resolve_map_lookup(tex1ResolveMap, reasset_id(entry->owner, texID));
                    if (resolvedID != -1) {
                        *texIDPtr = resolvedID;
                    } else {
                        *texIDPtr = 16; // fallback texture to make it obvious it's a bad texture

                        reasset_log_warning("[reasset] WARN: Failed to patch model (%s:%d) texture %d ID 0x%X. Texture was not defined!\n",
                            namespaceName, identifier, k, texID);
                    }
                }
            }
        }

        u32 modanimsSize;
        s16 *modanims = bin_ptr_get(&entry->modanimsPtr, &modanimsSize);
        if (modanims != NULL) {
            // TODO: anims
        }
    }

    // Patch in resolved model indices
    s32 numIndices = list_get_length(&modelIndexList);
    for (s32 i = 0; i < numIndices; i++) {
        ModelIndexEntry *entry = list_get(&modelIndexList, i);

        s16 *indexPtr = entry->binPtr.ptr;
        if (indexPtr == NULL) {
            continue;
        }

        s32 tabIdx = reasset_resolve_map_lookup(modelResolveMap, entry->modelID);
        if (tabIdx != -1) {
            *indexPtr = (s16)tabIdx;
        } else {
            *indexPtr = 0;

            const char *namespaceName;
            s32 identifier;
            reasset_id_lookup_name(entry->id, &namespaceName, &identifier);
            const char *modelNamespaceName;
            s32 modIdentifier;
            reasset_id_lookup_name(entry->id, &modelNamespaceName, &modIdentifier);
            reasset_log_warning("[reasset] WARN: Failed to patch model index (%s:%d) model ID %s:%d. Model was not defined!\n",
                namespaceName, identifier, 
                modelNamespaceName, modIdentifier);
        }
    }
}

void reasset_models_cleanup(void) {
    list_free(&modelList);
    u32list_free(&modelIDList);
    recomputil_destroy_u32_value_hashmap(modelMap);

    list_free(&modelIndexList);
    u32list_free(&modelIndexIDList);
    recomputil_destroy_u32_value_hashmap(modelIndexMap);
}

// MARK: Models

static void assert_custom_model_id(const char *funcName, ReAssetID id) {
    ReAssetIDData *idData = reasset_id_lookup_data(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        return;
    }

    if (idData->identifier >= 0 && idData->identifier <= modelOriginalCount) {
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_error("[reasset:%s] Custom model identifier %s:%d cannot overlap base model IDs. Reserved IDs: 0-%d.",
            funcName,
            namespaceName, idData->identifier, modelOriginalCount);
    }
}

RECOMP_EXPORT void reasset_models_set(ReAssetID id, ReAssetNamespace owner, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_models_set");
    assert_custom_model_id("reasset_models_set", id);

    ModelEntry *entry = get_or_create_model(id);
    buffer_set(&entry->model, data, sizeBytes);
    entry->owner = owner;

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] Model set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT void reasset_models_set_modanims(ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_models_set_modanims");
    assert_custom_model_id("reasset_models_set_modanims", id);

    ModelEntry *entry = get_or_create_model(id);
    buffer_set(&entry->modanims, data, sizeBytes);

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] Model anims set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT void reasset_models_set_amap(ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_models_set_amap");
    assert_custom_model_id("reasset_models_set_amap", id);

    ModelEntry *entry = get_or_create_model(id);
    buffer_set(&entry->amap, data, sizeBytes);

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] Model amap set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT void* reasset_models_get(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_models_get");

    ModelEntry *entry = get_model(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->modelPtr, outSizeBytes);
    } else {
        return buffer_get(&entry->model, outSizeBytes);
    }
}

RECOMP_EXPORT void* reasset_models_get_modanims(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_models_get_modanims");

    ModelEntry *entry = get_model(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->modanimsPtr, outSizeBytes);
    } else {
        return buffer_get(&entry->modanims, outSizeBytes);
    }
}

RECOMP_EXPORT void* reasset_models_get_amap(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_models_get_amap");

    ModelEntry *entry = get_model(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->amapPtr, outSizeBytes);
    } else {
        return buffer_get(&entry->amap, outSizeBytes);
    }
}

RECOMP_EXPORT ReAssetIterator reasset_models_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_models_create_iterator");

    return reasset_iterator_create(&modelIDList);
}

RECOMP_EXPORT void reasset_models_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_models_link");
    assert_custom_model_id("reasset_models_link", id);

    reasset_resolve_map_link(modelResolveMap, id, externID);
    reasset_resolve_map_link(modanimResolveMap, id, externID);
    reasset_resolve_map_link(amapResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_models_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_models_get_resolve_map");

    return modelResolveMap;
}

RECOMP_EXPORT ReAssetResolveMap reasset_models_get_modanims_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_models_get_modanims_resolve_map");

    return modanimResolveMap;
}

RECOMP_EXPORT ReAssetResolveMap reasset_models_get_amap_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_models_get_amap_resolve_map");

    return amapResolveMap;
}

// MARK: Model Indices

static void assert_custom_model_index_id(const char *funcName, ReAssetID id) {
    ReAssetIDData *idData = reasset_id_lookup_data(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        return;
    }

    if (idData->identifier >= 0 && idData->identifier <= modelIndexOriginalCount) {
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_error("[reasset:%s] Custom model index identifier %s:%d cannot overlap base model index IDs. Reserved IDs: 0-%d.",
            funcName,
            namespaceName, idData->identifier, modelIndexOriginalCount);
    }
}

_Bool reasset_model_indices_is_base_id(s32 id) {
    return id >= 0 && id < modelIndexOriginalCount;
}

RECOMP_EXPORT void reasset_model_indices_set(ReAssetID id, ReAssetID modelID) {
    reasset_assert_stage_set_call("reasset_model_indices_set");
    assert_custom_model_index_id("reasset_model_indices_set", id);

    ModelIndexEntry *entry = get_or_create_model_index(id);
    entry->modelID = modelID;

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] Model index set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT _Bool reasset_model_indices_get(ReAssetID id, ReAssetID *outModelID) {
    reasset_assert_stage_get_call("reasset_model_indices_get");

    ModelIndexEntry *entry = get_model_index(id);
    if (entry == NULL) {
        return FALSE;
    }

    if (outModelID != NULL) {
        *outModelID = entry->modelID;
    }

    return TRUE;
}

RECOMP_EXPORT ReAssetIterator reasset_model_indices_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_model_indices_create_iterator");

    return reasset_iterator_create(&modelIndexIDList);
}

RECOMP_EXPORT void reasset_model_indices_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_model_indices_link");
    assert_custom_model_index_id("reasset_model_indices_link", id);

    reasset_resolve_map_link(modelIndexResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_model_indices_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_model_indices_get_resolve_map");

    return modelIndexResolveMap;
}
