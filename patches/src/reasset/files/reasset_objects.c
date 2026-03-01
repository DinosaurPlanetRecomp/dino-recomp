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

#include "PR/ultratypes.h"
#include "game/objects/object_def.h"
#include "sys/fs.h"
#include "sys/memory.h"

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    Buffer object;
} ObjectEntry;

static s32 objectOriginalCount;
static s32 *objectOriginalTab;
static List objectList; // list[ObjectEntry]
static U32List objectIDList; // list[ReAssetID]
static U32ValueHashmapHandle objectMap; // ReAssetID -> object list index
static ReAssetResolveMap objectResolveMap;

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
    objectOriginalCount = (reasset_fst_get_file_size(OBJECTS_TAB) / sizeof(s32)) - 2;
    objectOriginalTab = reasset_fst_alloc_load_file(OBJECTS_TAB, NULL);

    list_init(&objectList, sizeof(ObjectEntry), objectOriginalCount);
    u32list_init(&objectIDList, objectOriginalCount);
    list_set_element_free_callback(&objectList, object_list_element_free);
    objectMap = recomputil_create_u32_value_hashmap();
    objectResolveMap = reasset_resolve_map_create("Object");

    // Add base objects (preserving order)
    for (s32 i = 0; i < objectOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        ObjectEntry *entry = get_or_create_object(id);

        s32 offset = objectOriginalTab[i];
        s32 size = objectOriginalTab[i + 1] - offset;

        buffer_set_base(&entry->object, OBJECTS_BIN, offset, size);
    }
}

void reasset_objects_repack(void) {
    s32 newCount = list_get_length(&objectList);

    // Calculate new bin size
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

        reasset_resolve_map_resolve_id(objectResolveMap, entry->id, entry->owner, i, (u8*)newBin + offset);

        newTab[i] = offset;
        buffer_copy_to(&entry->object, newBin, offset);
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

void reasset_objects_patch(void) {
    // Patch in resolved IDs
    s32 numObjects = list_get_length(&objectList);
    for (s32 i = 0; i < numObjects; i++) {
        ObjectEntry *entry = list_get(&objectList, i);
        if (entry->owner == REASSET_BASE_NAMESPACE) {
            continue;
        }

        ObjDef *object;
        if (reasset_resolve_map_lookup_ptr(objectResolveMap, entry->id, (void**)&object) == -1) {
            continue;
        }

        const char *namespaceName;
        s32 identifier;
        reasset_id_lookup_name(entry->id, &namespaceName, &identifier);

        // Patch DLL ID
        if (!reasset_dlls_is_base_id(object->dllID)) {
            s32 resolvedDLL = reasset_dlls_lookup(reasset_id(entry->owner, object->dllID));
            if (resolvedDLL != -1) {
                object->dllID = resolvedDLL;
            } else {
                object->dllID = 0;
                reasset_log_warning("[reasset] WARN: Failed to patch object (%s:%d) DLL ID 0x%X. DLL was not defined!\n",
                    namespaceName, identifier, object->dllID);
            }
        }

        // TODO: many other things to patch...
    }
}

void reasset_objects_cleanup(void) {
    list_free(&objectList);
    u32list_free(&objectIDList);
    recomputil_destroy_u32_value_hashmap(objectMap);
    recomp_free(objectOriginalTab);
    objectOriginalTab = NULL;
}

static void assert_custom_object_id(const char *funcName, ReAssetID id) {
    ReAssetIDData *idData = reasset_id_lookup_data(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        return;
    }

    if (idData->identifier >= 0 && idData->identifier <= objectOriginalCount) {
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_error("[reasset:%s] Custom object identifier %s:%d cannot overlap base object IDs. Reserved IDs: 0-%d.",
            funcName,
            namespaceName, idData->identifier, objectOriginalCount);
    }
}

RECOMP_EXPORT void reasset_objects_set(ReAssetID id, ReAssetNamespace owner, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_objects_set");
    assert_custom_object_id("reasset_objects_set", id);

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

    return buffer_get(&entry->object, outSizeBytes);
}

RECOMP_EXPORT ReAssetIterator reasset_objects_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_objects_create_iterator");

    return reasset_iterator_create(&objectIDList);
}

RECOMP_EXPORT void reasset_objects_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_objects_link");
    assert_custom_object_id("reasset_objects_link", id);

    reasset_resolve_map_link(objectResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_objects_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_objects_get_resolve_map");

    return objectResolveMap;
}
