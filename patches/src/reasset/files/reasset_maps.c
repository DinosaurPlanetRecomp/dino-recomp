#include "reasset_maps.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_iterator.h"
#include "reasset/list.h"
#include "reasset/buffer.h"
#include "reasset/bin_ptr.h"

#include "PR/ultratypes.h"
#include "libc/string.h"
#include "game/objects/object_id.h"
#include "sys/fs.h"
#include "sys/map.h"
#include "sys/memory.h"
#include "macros.h"

#define MAX_OBJ_GROUPS 32

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    Buffer header;
    BinPtr headerPtr;
    Buffer blocks;
    BinPtr blocksPtr;
    Buffer gridA1;
    BinPtr gridA1Ptr;
    Buffer gridA2;
    BinPtr gridA2Ptr;
    struct {
        List list; // list[MapObjectEntry]
        U32List idList; // list[ReAssetID]
        U32ValueHashmapHandle map; // ReAssetID -> object list index
        s32 maxUID;
    } objects;
    Buffer gridB1;
    BinPtr gridB1Ptr;
    Buffer gridB2;
    BinPtr gridB2Ptr;
} MapEntry;

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    Buffer object;
    BinPtr objectPtr;
    _Bool delete;
} MapObjectEntry;

static s32 mapOriginalCount;
static s32 *mapOriginalTab;
static List mapList; // list[MapEntry]
static U32List mapIDList; // list[ReAssetID]
static U32ValueHashmapHandle mapMap; // ReAssetID -> map list index
static ReAssetResolveMap mapResolveMap;
static U32ValueHashmapHandle mapObjectResolveMapMap; // ReAssetID -> ReAssetResolveMap
static U32ValueHashmapHandle mapObjectOriginalSetMap; // ReAssetID -> U32HashsetHandle (ReAssetID)

static void map_object_list_element_free(void *element) {
    MapObjectEntry *patch = element;
    buffer_free(&patch->object);
}

static MapObjectEntry* get_map_object(MapEntry *map, ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(map->objects.map, id, &listIdx)) {
        return list_get(&map->objects.list, listIdx);
    }

    return NULL;
}

static MapObjectEntry* get_or_create_map_object(MapEntry *map, ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(map->objects.map, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (reassetStage >= REASSET_STAGE_SET && idData->namespace == REASSET_BASE_NAMESPACE) {
            ReAssetIDData *mapIDData = reasset_id_lookup_data(map->id);
            reasset_error("[reasset] Attempted to patch out-of-bounds base map object. Map: %d, Obj UID: 0x%X", 
                mapIDData->identifier, idData->identifier);
        }

        u32list_add(&map->objects.idList, id);

        listIdx = list_get_length(&map->objects.list);
        
        MapObjectEntry *entry = list_add(&map->objects.list);
        entry->id = id;
        entry->owner = idData->namespace;
        buffer_init(&entry->object, 0);

        recomputil_u32_value_hashmap_insert(map->objects.map, id, listIdx);
    }

    return list_get(&map->objects.list, listIdx);
}

static void create_map_object_resolve_map(ReAssetID id) {
    if (recomputil_u32_value_hashmap_contains(mapObjectResolveMapMap, id)) {
        // Already exists
        return;
    }

    ReAssetIDData *idData = reasset_id_lookup_data(id);

    // Make name
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    const char *resolveMapNameTemp = recomp_sprintf_helper("MapObject (%s:%d)", namespaceName, idData->identifier);
    u32 resolveMapNameLen = strlen(resolveMapNameTemp);
    char *resolveMapName = recomp_alloc(resolveMapNameLen + 1);
    bcopy(resolveMapNameTemp, resolveMapName, resolveMapNameLen);
    resolveMapName[resolveMapNameLen] = '\0';
    
    // Add
    recomputil_u32_value_hashmap_insert(mapObjectResolveMapMap, id, reasset_resolve_map_create(resolveMapName));
}

static MapEntry* get_map(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(mapMap, id, &listIdx)) {
        return list_get(&mapList, listIdx);
    }

    return NULL;
}

static MapEntry* get_or_create_map(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(mapMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < mapOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base map: %d", idData->identifier);
        }

        u32list_add(&mapIDList, id);

        // Create map entry
        listIdx = list_get_length(&mapList);
        
        MapEntry *entry = list_add(&mapList);
        entry->id = id;
        entry->owner = idData->namespace;
        buffer_init(&entry->header, 0);
        buffer_init(&entry->blocks, 0);
        buffer_init(&entry->gridA1, 0);
        buffer_init(&entry->gridA2, 0);
        list_init(&entry->objects.list, sizeof(MapObjectEntry), 0);
        u32list_init(&entry->objects.idList, 0);
        list_set_element_free_callback(&entry->objects.list, map_object_list_element_free);
        entry->objects.map = recomputil_create_u32_value_hashmap();
        buffer_init(&entry->gridB1, 0);
        buffer_init(&entry->gridB2, 0);

        recomputil_u32_value_hashmap_insert(mapMap, id, listIdx);

        // Create resolve map for object setup list
        create_map_object_resolve_map(id);

        // If base, create a hashset for original object UIDs
        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            recomputil_u32_value_hashmap_insert(mapObjectOriginalSetMap, id, recomputil_create_u32_hashset());
        }
    }

    return list_get(&mapList, listIdx);
}

static void map_list_element_free(void *element) {
    MapEntry *entry = element;
    buffer_free(&entry->header);
    buffer_free(&entry->blocks);
    buffer_free(&entry->gridA1);
    buffer_free(&entry->gridA2);
    list_free(&entry->objects.list);
    u32list_free(&entry->objects.idList);
    recomputil_destroy_u32_value_hashmap(entry->objects.map);
    buffer_free(&entry->gridB1);
    buffer_free(&entry->gridB2);
}

void reasset_maps_init(void) {
    mapOriginalCount = reasset_fst_get_file_size(MAPS_TAB) / (sizeof(u32) * 7);
    mapOriginalTab = reasset_fst_alloc_load_file(MAPS_TAB, NULL);

    list_init(&mapList, sizeof(MapEntry), mapOriginalCount);
    u32list_init(&mapIDList, mapOriginalCount);
    list_set_element_free_callback(&mapList, map_list_element_free);
    mapMap = recomputil_create_u32_value_hashmap();
    mapResolveMap = reasset_resolve_map_create("Map");
    mapObjectResolveMapMap = recomputil_create_u32_value_hashmap();
    mapObjectOriginalSetMap = recomputil_create_u32_value_hashmap();

    // Add base maps (preserving order)
    Buffer objectSubfileTempBuffer = {0};
    buffer_init(&objectSubfileTempBuffer, 0);

    for (s32 i = 0; i < mapOriginalCount; i++) {
        s32 tabIndex = i * 7;

        ReAssetID id = reasset_base_id(i);
        MapEntry *entry = get_or_create_map(id);
        U32HashsetHandle originalObjectSet;
        reasset_assert(recomputil_u32_value_hashmap_get(mapObjectOriginalSetMap, id, &originalObjectSet),
            "[reasset] bug! Map %d object original set hashmap get failed.", i);

        u32 idx;

        idx = tabIndex + 0;
        buffer_set_base(&entry->header, MAPS_BIN, mapOriginalTab[idx], mapOriginalTab[idx + 1] - mapOriginalTab[idx]);

        idx = tabIndex + 1;
        buffer_set_base(&entry->blocks, MAPS_BIN, mapOriginalTab[idx], mapOriginalTab[idx + 1] - mapOriginalTab[idx]);

        idx = tabIndex + 2;
        buffer_set_base(&entry->gridA1, MAPS_BIN, mapOriginalTab[idx], mapOriginalTab[idx + 1] - mapOriginalTab[idx]);

        idx = tabIndex + 3;
        buffer_set_base(&entry->gridA2, MAPS_BIN, mapOriginalTab[idx], mapOriginalTab[idx + 1] - mapOriginalTab[idx]);

        idx = tabIndex + 4;
        buffer_load_from_file(&objectSubfileTempBuffer, MAPS_BIN, mapOriginalTab[idx], mapOriginalTab[idx + 1] - mapOriginalTab[idx]);

        entry->objects.maxUID = 0;

        u32 objSubfileSize;
        void *objSubfile = buffer_get(&objectSubfileTempBuffer, &objSubfileSize);
        u32 objoffset = 0;
        while (objoffset < objSubfileSize) {
            ObjSetup *setup = (ObjSetup*)((u8*)objSubfile + objoffset);
            u32 setupSize = setup->quarterSize * 4;

            if (setupSize == 0) {
                reasset_log_error("[reasset] Map %d contains a zero size object setup (offset 0x%X)!\n", i, objoffset);
                break;
            }

            ReAssetID setupID = reasset_base_id(setup->uID);
            MapObjectEntry *objectEntry = get_or_create_map_object(entry, setupID);

            buffer_set_base(&objectEntry->object, MAPS_BIN, mapOriginalTab[idx] + objoffset, setupSize);

            if (setup->uID > entry->objects.maxUID) {
                entry->objects.maxUID = setup->uID;
            }

            recomputil_u32_hashset_insert(originalObjectSet, setupID);

            objoffset += setupSize;
        }

        idx = tabIndex + 5;
        buffer_set_base(&entry->gridB1, MAPS_BIN, mapOriginalTab[idx], mapOriginalTab[idx + 1] - mapOriginalTab[idx]);

        idx = tabIndex + 6;
        buffer_set_base(&entry->gridB2, MAPS_BIN, mapOriginalTab[idx], mapOriginalTab[idx + 1] - mapOriginalTab[idx]);
    }

    buffer_free(&objectSubfileTempBuffer);
}

void reasset_maps_repack(void) {
    s32 newCount = list_get_length(&mapList);

    // Calculate new bin size
    u32 newBinSize = 0;
    for (s32 i = 0; i < newCount; i++) {
        MapEntry *entry = list_get(&mapList, i);
        
        newBinSize += sizeof(MapHeader);
        newBinSize += buffer_get_size(&entry->blocks);
        newBinSize += buffer_get_size(&entry->gridA1);
        newBinSize += buffer_get_size(&entry->gridA2);
        s32 numObjects = list_get_length(&entry->objects.list);
        for (s32 k = 0; k < numObjects; k++) {
            MapObjectEntry *objEntry = list_get(&entry->objects.list, k);
            if (!objEntry->delete) {
                newBinSize += buffer_get_size(&objEntry->object);
            }
        }
        newBinSize += buffer_get_size(&entry->gridB1);
        newBinSize += buffer_get_size(&entry->gridB2);
        newBinSize = mmAlign16(newBinSize);
    }

    // Alloc new MAPS.tab/bin
    u32 newTabSize = ((newCount * 7) + 2) * sizeof(s32);
    s32 *newTab = recomp_alloc(newTabSize);
    void *newBin = recomp_alloc(newBinSize);
    bzero(newBin, newBinSize);

    // Rebuild
    U32List objSetupLists[1 + MAX_OBJ_GROUPS + 1] = {0};
    for (u32 i = 0; i < ARRAYCOUNT(objSetupLists); i++) {
        u32list_init(&objSetupLists[i], 0);
    }
    s32 offset = 0;
    for (s32 i = 0; i < newCount; i++) {
        MapEntry *entry = list_get(&mapList, i);
        ReAssetIDData *idData = reasset_id_lookup_data(entry->id);
        s32 tabIndex = i * 7;

        if (idData->namespace != REASSET_BASE_NAMESPACE) {
            const char *namespaceName;
            reasset_namespace_lookup_name(idData->namespace, &namespaceName);
            reasset_log("[reasset] New map: %s:%d\n", namespaceName, idData->identifier);
        }

        // Sort object setups
        s32 numObjects = list_get_length(&entry->objects.list);
        s32 newNumObjects = 0;
        for (s32 k = 0; k < numObjects; k++) {
            MapObjectEntry *objEntry = list_get(&entry->objects.list, k);

            if (objEntry->delete) {
                ReAssetIDData *objIDData = reasset_id_lookup_data(objEntry->id);
                const char *namespaceName;
                reasset_namespace_lookup_name(objIDData->namespace, &namespaceName);
                reasset_log("[reasset] Deleted map object %s:%d\n", 
                    namespaceName, objIDData->identifier);
                continue;
            }

            u32 setupSize;
            ObjSetup *setup = buffer_get(&objEntry->object, &setupSize);
            if (setupSize < sizeof(ObjSetup)) {
                ReAssetIDData *objIDData = reasset_id_lookup_data(objEntry->id);
                const char *namespaceName;
                reasset_namespace_lookup_name(objIDData->namespace, &namespaceName);
                reasset_log_error("[reasset] Map object %s:%d setup data is too small! Expected >= %d, got %d. Object will be skipped!\n", 
                    namespaceName, objIDData->identifier,
                    sizeof(ObjSetup), setupSize);
                continue;
            }

            if (setup->objId == OBJ_curve || setup->objId == OBJ_checkpoint4) {
                // Curve/checkpoint
                u32list_add(&objSetupLists[1 + MAX_OBJ_GROUPS], k);
            } else if (setup->loadFlags & OBJSETUP_LOAD_IN_MAP_OBJGROUP) {
                // Grouped object
                if (setup->mapObjGroup >= MAX_OBJ_GROUPS) {
                    ReAssetIDData *objIDData = reasset_id_lookup_data(objEntry->id);
                    const char *namespaceName;
                    reasset_namespace_lookup_name(objIDData->namespace, &namespaceName);
                    reasset_log_error("[reasset] Map object %s:%d setup has an out of bounds group number: %d. Object will be left ungrouped!\n", 
                        namespaceName, objIDData->identifier,
                        setup->mapObjGroup);
                    u32list_add(&objSetupLists[0], k);
                } else {
                    u32list_add(&objSetupLists[1 + setup->mapObjGroup], k);
                }
            } else {
                // Ungrouped object
                u32list_add(&objSetupLists[0], k);
            }

            newNumObjects += 1;
        }

        // Resolve map itself
        reasset_resolve_map_resolve_id(mapResolveMap, entry->id, entry->owner, i);

        // Header
        newTab[tabIndex + 0] = offset;
        bin_ptr_set(&entry->headerPtr, newBin, offset, sizeof(MapHeader));
        buffer_copy_to_bin_ptr(&entry->header, &entry->headerPtr);

        MapHeader *header = (MapHeader*)((u8*)newBin + offset);
        header->objectInstanceCount = newNumObjects;

        offset += sizeof(MapHeader);

        // Blocks
        newTab[tabIndex + 1] = offset;
        bin_ptr_set(&entry->blocksPtr, newBin, offset, buffer_get_size(&entry->blocks));
        buffer_copy_to_bin_ptr(&entry->blocks, &entry->blocksPtr);
        offset += buffer_get_size(&entry->blocks);

        // Grid A1
        newTab[tabIndex + 2] = offset;
        bin_ptr_set(&entry->gridA1Ptr, newBin, offset, buffer_get_size(&entry->gridA1));
        buffer_copy_to_bin_ptr(&entry->gridA1, &entry->gridA1Ptr);
        offset += buffer_get_size(&entry->gridA1);

        // Grid A2
        newTab[tabIndex + 3] = offset;
        bin_ptr_set(&entry->gridA2Ptr, newBin, offset, buffer_get_size(&entry->gridA2));
        buffer_copy_to_bin_ptr(&entry->gridA2, &entry->gridA2Ptr);
        offset += buffer_get_size(&entry->gridA2);

        // Objects
        newTab[tabIndex + 4] = offset;
        s32 nextUID = entry->objects.maxUID + 1;
        ReAssetResolveMap objResolveMap;
        reasset_assert(recomputil_u32_value_hashmap_get(mapObjectResolveMapMap, entry->id, &objResolveMap),
            "[reasset] bug! Map %d object resolve map hashmap get failed.", i);
        for (u32 k = 0; k < ARRAYCOUNT(objSetupLists); k++) {
            U32List *objList = &objSetupLists[k]; 
            s32 numGroupObjects = u32list_get_length(objList);
            for (s32 j = 0; j < numGroupObjects; j++) {
                MapObjectEntry *objEntry = list_get(&entry->objects.list, u32list_get(objList, j));

                u32 setupSize = mmAlign4(buffer_get_size(&objEntry->object));
                bin_ptr_set(&objEntry->objectPtr, newBin, offset, setupSize);
                buffer_copy_to_bin_ptr(&objEntry->object, &objEntry->objectPtr);

                ObjSetup *setup = (ObjSetup*)((u8*)newBin + offset);
                // Always recalculate size
                setup->quarterSize = setupSize / 4;

                // Assign UID for custom assets
                ReAssetIDData *objIDData = reasset_id_lookup_data(objEntry->id);
                if (objIDData->namespace != REASSET_BASE_NAMESPACE) {
                    setup->uID = nextUID;
                    nextUID += 1;
                }

                // Resolve object setup
                reasset_resolve_map_resolve_id(objResolveMap, objEntry->id, objEntry->owner, setup->uID);

                offset += setupSize;
            }
        }

        // Grid B1
        newTab[tabIndex + 5] = offset;
        bin_ptr_set(&entry->gridB1Ptr, newBin, offset, buffer_get_size(&entry->gridB1));
        buffer_copy_to_bin_ptr(&entry->gridB1, &entry->gridB1Ptr);
        offset += buffer_get_size(&entry->gridB1);

        // Grid B2
        newTab[tabIndex + 6] = offset;
        bin_ptr_set(&entry->gridB2Ptr, newBin, offset, buffer_get_size(&entry->gridB2));
        buffer_copy_to_bin_ptr(&entry->gridB2, &entry->gridB2Ptr);
        offset += buffer_get_size(&entry->gridB2);

        offset = mmAlign16(offset);

        for (u32 i = 0; i < ARRAYCOUNT(objSetupLists); i++) {
            u32list_clear(&objSetupLists[i]);
        }
    }

    s32 endIndex = newCount * 7;
    newTab[endIndex] = offset;
    newTab[endIndex + 1] = -1;

    // Finalize resolve map
    reasset_resolve_map_finalize(mapResolveMap);

    // Set new files
    reasset_fst_set_internal(MAPS_TAB, newTab, newTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(MAPS_BIN, newBin, newBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt MAPS.tab & MAPS.bin (count: %d, bin size: 0x%X).\n", newCount, newBinSize);

    // Clean up
    for (u32 i = 0; i < ARRAYCOUNT(objSetupLists); i++) {
        u32list_free(&objSetupLists[i]);
    }
    list_free(&mapList);
    u32list_free(&mapIDList);
    recomputil_destroy_u32_value_hashmap(mapMap);
    recomp_free(mapOriginalTab);
    mapOriginalTab = NULL;
}

// MARK: Maps

static void assert_custom_map_id(const char *funcName, ReAssetID id) {
    ReAssetIDData *idData = reasset_id_lookup_data(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        return;
    }

    if (idData->identifier >= 0 && idData->identifier <= mapOriginalCount) {
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_error("[reasset:%s] Custom map identifier %s:%d cannot overlap base map IDs. Reserved IDs: 0-%d.",
            funcName,
            namespaceName, idData->identifier, mapOriginalCount);
    }
}

static void log_map_set(ReAssetID id, const char *msg) {
    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] %s: %s:%d\n", msg, namespaceName, idData->identifier);
}

RECOMP_EXPORT void reasset_maps_set_header(ReAssetID id, ReAssetNamespace owner, const void *data) {
    reasset_assert_stage_set_call("reasset_maps_set_header");
    assert_custom_map_id("reasset_maps_set_header", id);

    MapEntry *entry = get_or_create_map(id);
    buffer_set(&entry->header, data, sizeof(MapHeader));
    entry->owner = owner;

    log_map_set(id, "Map header set");
}

RECOMP_EXPORT void* reasset_maps_get_header(ReAssetID id) {
    reasset_assert_stage_get_call("reasset_maps_get_header");

    MapEntry *entry = get_map(id);
    if (entry == NULL) {
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->headerPtr, NULL);
    }

    if (buffer_get_size(&entry->header) == 0) {
        buffer_zero(&entry->header, sizeof(MapHeader));
    }

    return buffer_get(&entry->header, NULL);
}

RECOMP_EXPORT void reasset_maps_set_blocks(ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_maps_set_blocks");
    assert_custom_map_id("reasset_maps_set_blocks", id);

    MapEntry *entry = get_or_create_map(id);
    buffer_set(&entry->blocks, data, sizeBytes);

    log_map_set(id, "Map blocks set");
}

RECOMP_EXPORT void* reasset_maps_get_blocks(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_maps_get_blocks");

    MapEntry *entry = get_map(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->blocksPtr, outSizeBytes);
    } else {
        return buffer_get(&entry->blocks, outSizeBytes);
    }
}

RECOMP_EXPORT void reasset_maps_set_grid_a1(ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_maps_set_grid_a1");
    assert_custom_map_id("reasset_maps_set_grid_a1", id);

    MapEntry *entry = get_or_create_map(id);
    buffer_set(&entry->gridA1, data, sizeBytes);

    log_map_set(id, "Map grid A1 set");
}

RECOMP_EXPORT void* reasset_maps_get_grid_a1(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_maps_get_grid_a1");

    MapEntry *entry = get_map(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->gridA1Ptr, outSizeBytes);
    } else {
        return buffer_get(&entry->gridA1, outSizeBytes);
    }
}

RECOMP_EXPORT void reasset_maps_set_grid_a2(ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_maps_set_grid_a2");
    assert_custom_map_id("reasset_maps_set_grid_a2", id);

    MapEntry *entry = get_or_create_map(id);
    buffer_set(&entry->gridA2, data, sizeBytes);

    log_map_set(id, "Map grid A2 set");
}

RECOMP_EXPORT void* reasset_maps_get_grid_a2(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_maps_get_grid_a2");

    MapEntry *entry = get_map(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->gridA2Ptr, outSizeBytes);
    } else {
        return buffer_get(&entry->gridA2, outSizeBytes);
    }
}

RECOMP_EXPORT void reasset_maps_set_grid_b1(ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_maps_set_grid_b1");
    assert_custom_map_id("reasset_maps_set_grid_b1", id);

    MapEntry *entry = get_or_create_map(id);
    buffer_set(&entry->gridB1, data, sizeBytes);

    log_map_set(id, "Map grid B1 set");
}

RECOMP_EXPORT void* reasset_maps_get_grid_b1(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_maps_get_grid_b1");

    MapEntry *entry = get_map(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->gridB1Ptr, outSizeBytes);
    } else {
        return buffer_get(&entry->gridB1, outSizeBytes);
    }
}

RECOMP_EXPORT void reasset_maps_set_grid_b2(ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_maps_set_grid_b2");
    assert_custom_map_id("reasset_maps_set_grid_b2", id);

    MapEntry *entry = get_or_create_map(id);
    buffer_set(&entry->gridB2, data, sizeBytes);

    log_map_set(id, "Map grid B2 set");
}

RECOMP_EXPORT void* reasset_maps_get_grid_b2(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_maps_get_grid_b2");

    MapEntry *entry = get_map(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->gridB2Ptr, outSizeBytes);
    } else {
        return buffer_get(&entry->gridB2, outSizeBytes);
    }
}

RECOMP_EXPORT ReAssetIterator reasset_maps_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_maps_create_iterator");

    return reasset_iterator_create(&mapIDList);
}

RECOMP_EXPORT void reasset_maps_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_maps_link");
    assert_custom_map_id("reasset_maps_link", id);

    reasset_resolve_map_link(mapResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_maps_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_maps_get_resolve_map");

    return mapResolveMap;
}

// MARK: Map Objects

_Bool reasset_map_objects_is_base_uid(ReAssetID mapID, s32 uid) {
    U32HashsetHandle originalObjectSet;
    if (recomputil_u32_value_hashmap_get(mapObjectOriginalSetMap, mapID, &originalObjectSet)) {
        ReAssetID id = reasset_base_id(uid);
        return recomputil_u32_hashset_contains(originalObjectSet, id);
    }

    return FALSE;
}

RECOMP_EXPORT void reasset_map_objects_set(ReAssetID mapID, ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_map_objects_set");

    MapEntry *entry = get_or_create_map(mapID);
    MapObjectEntry *objEntry = get_or_create_map_object(entry, id);
    
    buffer_set(&objEntry->object, data, sizeBytes);

    // Logging
    ReAssetIDData *mapIDData = reasset_id_lookup_data(mapID);
    ReAssetIDData *objIDData = reasset_id_lookup_data(id);
    const char *mapNamespaceName;
    reasset_namespace_lookup_name(mapIDData->namespace, &mapNamespaceName);
    const char *objNamespaceName;
    reasset_namespace_lookup_name(objIDData->namespace, &objNamespaceName);
    reasset_log("[reasset] Map object set: %s:%d[%s:%d]\n", 
        mapNamespaceName, mapIDData->identifier,
        objNamespaceName, objIDData->identifier);
}

RECOMP_EXPORT void* reasset_map_objects_get(ReAssetID mapID, ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_map_objects_get");

    MapEntry *entry = get_map(mapID);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    MapObjectEntry *objEntry = get_map_object(entry, id);
    if (objEntry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&objEntry->objectPtr, outSizeBytes);
    }

    if (buffer_get_size(&objEntry->object) == 0) {
        buffer_zero(&objEntry->object, sizeof(ObjSetup));

        // Default blank setups to DummyObject to be safe
        ObjSetup *setup = buffer_get(&objEntry->object, NULL);
        setup->objId = OBJ_DummyObject;
    }

    return buffer_get(&objEntry->object, outSizeBytes);
}

RECOMP_EXPORT void reasset_map_objects_delete(ReAssetID mapID, ReAssetID id) {
    reasset_assert_stage_delete_call("reasset_map_objects_delete");

    MapEntry *entry = get_map(mapID);
    if (entry == NULL) {
        return;
    }

    MapObjectEntry *objEntry = get_map_object(entry, id);
    if (objEntry == NULL) {
        return;
    }

    objEntry->delete = TRUE;
}

RECOMP_EXPORT ReAssetIterator reasset_map_objects_create_iterator(ReAssetID mapID) {
    reasset_assert_stage_iterator_call("reasset_map_objects_create_iterator");

    MapEntry *entry = get_map(mapID);
    if (entry == NULL) {
        return reasset_iterator_create(NULL);
    }

    return reasset_iterator_create(&entry->objects.idList);
}


RECOMP_EXPORT void reasset_map_objects_link(ReAssetID mapID, ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_map_objects_link");

    create_map_object_resolve_map(mapID);

    ReAssetResolveMap objResolveMap;
    reasset_assert(recomputil_u32_value_hashmap_get(mapObjectResolveMapMap, mapID, &objResolveMap),
        "[reasset] bug! Map object resolve map hashmap get failed in link call.");

    reasset_resolve_map_link(objResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_map_objects_get_resolve_map(ReAssetID mapID) {
    reasset_assert_stage_get_resolve_map_call("reasset_maps_get_resolve_map");

    create_map_object_resolve_map(mapID);

    ReAssetResolveMap objResolveMap;
    reasset_assert(recomputil_u32_value_hashmap_get(mapObjectResolveMapMap, mapID, &objResolveMap),
        "[reasset] bug! Map object resolve map hashmap get failed in get resolve map call.");

    return objResolveMap;
}
