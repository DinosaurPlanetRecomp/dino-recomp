#include "reasset_objects.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_iterator.h"
#include "reasset/special/reasset_dlls.h"
#include "reasset/buffer.h"
#include "reasset/list.h"
#include "reasset/bin_ptr.h"

#include "PR/ultratypes.h"
#include "game/objects/object_def.h"
#include "sys/fs.h"
#include "sys/memory.h"

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    Buffer object;
    BinPtr objectPtr;
} ObjectEntry;

typedef struct {
    ReAssetID id;
    ReAssetID objID;
    BinPtr binPtr;
} ObjectIndexEntry;

static s32 objectOriginalCount;
static List objectList; // list[ObjectEntry]
static U32List objectIDList; // list[ReAssetID]
static U32ValueHashmapHandle objectMap; // ReAssetID -> object list index
static ReAssetResolveMap objectResolveMap;

static s32 objectIndexOriginalCount;
static List objectIndexList; // list[ObjectIndexEntry]
static U32List objectIndexIDList; // list[ReAssetID]
static U32ValueHashmapHandle objectIndexMap; // ReAssetID -> object index list index
static ReAssetResolveMap objectIndexResolveMap;

static ObjectIndexEntry* get_object_index(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(objectIndexMap, id, &listIdx)) {
        return list_get(&objectIndexList, listIdx);
    }

    return NULL;
}

static ObjectIndexEntry* get_or_create_object_index(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(objectIndexMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < objectIndexOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base object index: %d", idData->identifier);
        }

        u32list_add(&objectIndexIDList, id);

        listIdx = list_get_length(&objectIndexList);
        
        ObjectIndexEntry *entry = list_add(&objectIndexList);
        entry->id = id;

        recomputil_u32_value_hashmap_insert(objectIndexMap, id, listIdx);
    }

    return list_get(&objectIndexList, listIdx);
}

static ObjectEntry* get_object(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(objectMap, id, &listIdx)) {
        return list_get(&objectList, listIdx);
    }

    return NULL;
}

static ObjectEntry* get_or_create_object(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(objectMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < objectOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base object: %d", idData->identifier);
        }

        u32list_add(&objectIDList, id);

        listIdx = list_get_length(&objectList);
        
        ObjectEntry *entry = list_add(&objectList);
        entry->id = id;
        entry->owner = idData->namespace;
        buffer_init(&entry->object, 0);

        recomputil_u32_value_hashmap_insert(objectMap, id, listIdx);
    }

    return list_get(&objectList, listIdx);
}

static void object_list_element_free(void *element) {
    ObjectEntry *entry = element;
    buffer_free(&entry->object);
}

void reasset_objects_init(void) {
    // Objects
    objectOriginalCount = (reasset_fst_get_file_size(OBJECTS_TAB) / sizeof(s32)) - 2;
    s32 *originalObjTab = reasset_fst_alloc_load_file(OBJECTS_TAB, NULL);

    list_init(&objectList, sizeof(ObjectEntry), objectOriginalCount);
    list_set_element_free_callback(&objectList, object_list_element_free);
    u32list_init(&objectIDList, objectOriginalCount);
    objectMap = recomputil_create_u32_value_hashmap();
    objectResolveMap = reasset_resolve_map_create("Object");

    // Add base objects (preserving order)
    for (s32 i = 0; i < objectOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        ObjectEntry *entry = get_or_create_object(id);

        s32 offset = originalObjTab[i];
        s32 size = originalObjTab[i + 1] - offset;

        buffer_set_base(&entry->object, OBJECTS_BIN, offset, size);
    }

    recomp_free(originalObjTab);

    // Object Indices
    objectIndexOriginalCount = reasset_fst_get_file_size(OBJINDEX_BIN) / sizeof(s16);
    s16 *originalIndexTab = reasset_fst_alloc_load_file(OBJINDEX_BIN, NULL);

    list_init(&objectIndexList, sizeof(ObjectIndexEntry), objectIndexOriginalCount);
    u32list_init(&objectIndexIDList, objectIndexOriginalCount);
    objectIndexMap = recomputil_create_u32_value_hashmap();
    objectIndexResolveMap = reasset_resolve_map_create("ObjectIndex");

    // Add base object indices (preserving order)
    for (s32 i = 0; i < objectIndexOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        ObjectIndexEntry *entry = get_or_create_object_index(id);

        entry->objID = reasset_base_id(originalIndexTab[i]);
    }

    recomp_free(originalIndexTab);
}

static void reasset_objects_repack_internal(void) {
    s32 newCount = list_get_length(&objectList);

    // Calculate new OBJECTS.bin size
    u32 newBinSize = 0;
    for (s32 i = 0; i < newCount; i++) {
        ObjectEntry *entry = list_get(&objectList, i);
        
        newBinSize += buffer_get_size(&entry->object);
        newBinSize = mmAlign4(newBinSize);
    }

    // Alloc new OBJECTS.tab/bin
    u32 newTabSize = (newCount + 2) * sizeof(s32);
    s32 *newTab = recomp_alloc(newTabSize);
    void *newBin = recomp_alloc(newBinSize);
    bzero(newBin, newBinSize);

    // Rebuild
    s32 offset = 0;
    for (s32 i = 0; i < newCount; i++) {
        ObjectEntry *entry = list_get(&objectList, i);
        ReAssetIDData *idData = reasset_id_lookup_data(entry->id);

        if (idData->namespace != REASSET_BASE_NAMESPACE) {
            // Make copy and null-terminate ourselves just to be safe
            char name[16] = {0};
            ObjDef *obj = buffer_get(&entry->object, NULL);
            bcopy(obj->name, name, 15);
            name[15] = '\0';

            const char *namespaceName;
            reasset_namespace_lookup_name(idData->namespace, &namespaceName);
            reasset_log("[reasset] New object: %s:%d \"%s\"\n", 
                namespaceName, idData->identifier, name);
        }

        reasset_resolve_map_resolve_id(objectResolveMap, entry->id, entry->owner, i);

        newTab[i] = offset;
        bin_ptr_set(&entry->objectPtr, newBin, offset, buffer_get_size(&entry->object));
        buffer_copy_to_bin_ptr(&entry->object, &entry->objectPtr);
        offset += buffer_get_size(&entry->object);

        offset = mmAlign4(offset);
    }

    newTab[newCount] = offset;
    newTab[newCount + 1] = -1;

    // Finalize resolve map
    reasset_resolve_map_finalize(objectResolveMap);

    // Set new files
    reasset_fst_set_internal(OBJECTS_TAB, newTab, newTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(OBJECTS_BIN, newBin, newBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt OBJECTS.tab & OBJECTS.bin (count: %d, bin size: 0x%X).\n", newCount, newBinSize);
}

static void reasset_objects_indices_repack_internal(void) {
    s32 newCount = list_get_length(&objectIndexList);

    // Calculate new OBJINDEX.bin size
    u32 newIndexBinSize = newCount * sizeof(s16);

    // Alloc new OBJINDEX.bin
    s16 *newBin = recomp_alloc(newIndexBinSize);
    bzero(newBin, newIndexBinSize);

    // Rebuild
    for (s32 i = 0; i < newCount; i++) {
        ObjectIndexEntry *entry = list_get(&objectIndexList, i);
        ReAssetIDData *idData = reasset_id_lookup_data(entry->id);

        if (idData->namespace != REASSET_BASE_NAMESPACE) {
            const char *namespaceName;
            reasset_namespace_lookup_name(idData->namespace, &namespaceName);
            reasset_log("[reasset] New object index: %s:%d\n", 
                namespaceName, idData->identifier);
        }

        reasset_resolve_map_resolve_id(objectIndexResolveMap, entry->id, -1, i);

        bin_ptr_set(&entry->binPtr, newBin, i * sizeof(s16), sizeof(s16));

        // Note: Nothing to write to bin here. Values will be filled in during the patch stage
    }

    // Finalize resolve map
    reasset_resolve_map_finalize(objectIndexResolveMap);

    // Set new files
    reasset_fst_set_internal(OBJINDEX_BIN, newBin, newIndexBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt OBJINDEX.bin (count: %d, bin size: 0x%X).\n", newCount, newIndexBinSize);
}

void reasset_objects_repack(void) {
    reasset_objects_repack_internal();
    reasset_objects_indices_repack_internal();
}

void reasset_objects_patch(void) {
    // Patch in resolved IDs
    s32 numObjects = list_get_length(&objectList);
    for (s32 i = 0; i < numObjects; i++) {
        ObjectEntry *entry = list_get(&objectList, i);
        if (entry->owner == REASSET_BASE_NAMESPACE) {
            continue;
        }

        ObjDef *object = entry->objectPtr.ptr;
        if (object == NULL) {
            continue;
        }

        const char *namespaceName;
        s32 identifier;
        reasset_id_lookup_name(entry->id, &namespaceName, &identifier);

        // Patch DLL ID
        s32 resolvedDLL = reasset_dlls_lookup(reasset_id(entry->owner, object->dllID));
        if (resolvedDLL != -1) {
            object->dllID = resolvedDLL;
        } else if (!reasset_dlls_is_base_id(object->dllID)) {
            object->dllID = 0;
            reasset_log_warning("[reasset] WARN: Failed to patch object (%s:%d) DLL ID 0x%X. DLL was not defined!\n",
                namespaceName, identifier, object->dllID);
        }

        // TODO: many other things to patch...
    }

    // Patch in resolved object indices
    s32 numIndices = list_get_length(&objectIndexList);
    for (s32 i = 0; i < numIndices; i++) {
        ObjectIndexEntry *entry = list_get(&objectIndexList, i);

        s16 *indexPtr = entry->binPtr.ptr;
        if (indexPtr == NULL) {
            continue;
        }

        ReAssetIDData *objIDData = reasset_id_lookup_data(entry->objID);
        if (objIDData->namespace == REASSET_BASE_NAMESPACE && objIDData->identifier == -1) {
            *indexPtr = -1;
        } else {
            s32 tabIdx = reasset_resolve_map_lookup(objectResolveMap, entry->objID);
            if (tabIdx != -1) {
                *indexPtr = (s16)tabIdx;
            } else {
                *indexPtr = 0; // DummyObject

                const char *namespaceName;
                s32 identifier;
                reasset_id_lookup_name(entry->id, &namespaceName, &identifier);
                const char *objNamespaceName;
                s32 objIdentifier;
                reasset_id_lookup_name(entry->id, &objNamespaceName, &objIdentifier);
                reasset_log_warning("[reasset] WARN: Failed to patch object index (%s:%d) object ID %s:%d. Object was not defined!\n",
                    namespaceName, identifier, 
                    objNamespaceName, objIdentifier);
            }
        }
    }
}

void reasset_objects_cleanup(void) {
    list_free(&objectList);
    u32list_free(&objectIDList);
    recomputil_destroy_u32_value_hashmap(objectMap);

    list_free(&objectIndexList);
    u32list_free(&objectIndexIDList);
    recomputil_destroy_u32_value_hashmap(objectIndexMap);
}

// MARK: Objects

RECOMP_EXPORT void reasset_objects_set(ReAssetID id, ReAssetNamespace owner, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_objects_set");

    ObjectEntry *entry = get_or_create_object(id);
    buffer_set(&entry->object, data, sizeBytes);
    entry->owner = owner;

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] Object set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT void* reasset_objects_get(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_objects_get");

    ObjectEntry *entry = get_object(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->objectPtr, outSizeBytes);
    } else {
        return buffer_get(&entry->object, outSizeBytes);
    }
}

RECOMP_EXPORT ReAssetIterator reasset_objects_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_objects_create_iterator");

    return reasset_iterator_create(&objectIDList);
}

RECOMP_EXPORT void reasset_objects_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_objects_link");

    reasset_resolve_map_link(objectResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_objects_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_objects_get_resolve_map");

    return objectResolveMap;
}

// MARK: Object Indices

_Bool reasset_object_indices_is_base_id(s32 id) {
    return id >= 0 && id < objectIndexOriginalCount;
}

RECOMP_EXPORT void reasset_object_indices_set(ReAssetID id, ReAssetID objID) {
    reasset_assert_stage_set_call("reasset_object_indices_set");

    ObjectIndexEntry *entry = get_or_create_object_index(id);
    entry->objID = objID;

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] Object index set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT _Bool reasset_object_indices_get(ReAssetID id, ReAssetID *outObjID) {
    reasset_assert_stage_get_call("reasset_object_indices_get");

    ObjectIndexEntry *entry = get_object_index(id);
    if (entry == NULL) {
        return FALSE;
    }

    if (outObjID != NULL) {
        *outObjID = entry->objID;
    }

    return TRUE;
}

RECOMP_EXPORT ReAssetIterator reasset_object_indices_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_object_indices_create_iterator");

    return reasset_iterator_create(&objectIndexIDList);
}

RECOMP_EXPORT void reasset_object_indices_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_object_indices_link");

    reasset_resolve_map_link(objectIndexResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_object_indices_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_object_indices_get_resolve_map");

    return objectIndexResolveMap;
}
