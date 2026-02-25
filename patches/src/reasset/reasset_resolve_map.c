#include "reasset_resolve_map.h"

#include "patches.h"
#include "recompdata.h"

#include "reasset.h"
#include "reasset_id.h"
#include "reasset_namespace.h"
#include "list.h"

#include "PR/ultratypes.h"

typedef struct {
    ReAssetID id;
    ReAssetID externID;
} ReAssetIDLink;

typedef struct {
    s32 identifier;
    void *ptr;
    ReAssetNamespace owner;
} ReAssetResolvedID;

typedef struct {
    U32MemoryHashmapHandle idMap; // ReAssetID -> ReAssetResolvedID
    U32ValueHashmapHandle ownershipMap; // resolved identifier -> namespace
    List linkList; // list[ReAssetIDLink]
    U32ValueHashmapHandle linkMap; // ReAssetID -> link list index
    _Bool finalized;
    const char *assetTypeName;
} ReAssetResolveMapData;

static MemorySlotmapHandle sResolveMapSlotmap;

static void resolve_id_internal(ReAssetResolveMapData *data, ReAssetID id, ReAssetNamespace owner, s32 resolvedIdentifier, void *resolvedPtr);

void reasset_resolve_map_init(void) {
    sResolveMapSlotmap = recomputil_create_memory_slotmap(sizeof(ReAssetResolveMapData));
}

ReAssetResolveMap reasset_resolve_map_create(const char *assetTypeName) {
    ReAssetResolveMap map = recomputil_memory_slotmap_create(sResolveMapSlotmap);
    ReAssetResolveMapData *data;
    reasset_assert(recomputil_memory_slotmap_get(sResolveMapSlotmap, map, (void**)&data), 
        "[reasset] bug! reasset_resolve_map_create slotmap get failed.");

    data->finalized = FALSE;
    data->assetTypeName = assetTypeName;
    
    data->idMap = recomputil_create_u32_memory_hashmap(sizeof(ReAssetResolvedID));
    data->ownershipMap = recomputil_create_u32_value_hashmap();
    list_init(&data->linkList, sizeof(ReAssetIDLink), 0);
    data->linkMap = recomputil_create_u32_value_hashmap();

    return map;
}

void reasset_resolve_map_finalize(ReAssetResolveMap map) {
    ReAssetResolveMapData *data;
    reasset_assert(recomputil_memory_slotmap_get(sResolveMapSlotmap, map, (void**)&data), 
        "[reasset] Invalid resolve map %d given during finalize.", map);

    if (data->finalized) return;

    // Resolve links
    s32 numLinks = list_get_length(&data->linkList);
    for (s32 i = 0; i < numLinks; i++) {
        ReAssetIDLink *link = list_get(&data->linkList, i);
        if (recomputil_u32_memory_hashmap_contains(data->idMap, link->id)) {
            // Don't overwrite already resolved IDs. If an ID is linked but then defined, keep
            // the defined value and ignore the link to be safe.
            s32 identifier;
            const char *namespaceName;
            reasset_id_lookup_name(link->id, &namespaceName, &identifier);
            s32 externIdentifier;
            const char *externNamespaceName;
            reasset_id_lookup_name(link->externID, &externNamespaceName, &externIdentifier);
            reasset_log_warning("[reasset] WARN: Ignoring %s link %s:%d -> %s:%d. ID %s:%d was directly defined with a value.\n",
                data->assetTypeName,
                namespaceName, identifier,
                externNamespaceName, externIdentifier,
                namespaceName, identifier);
            continue;
        }
        if (recomputil_u32_value_hashmap_contains(data->linkMap, link->externID)) {
            // Don't allow linking to a link, we don't resolve these correctly
            s32 identifier;
            const char *namespaceName;
            reasset_id_lookup_name(link->id, &namespaceName, &identifier);
            reasset_log_warning("[reasset] WARN: Ignoring %s link %s:%d. Links to other links are not supported.\n",
                data->assetTypeName,
                namespaceName, identifier);
            continue;
        }

        ReAssetResolvedID *resolvedExtern = recomputil_u32_memory_hashmap_get(data->idMap, link->externID);
        if (resolvedExtern == NULL) {
            s32 identifier;
            const char *namespaceName;
            reasset_id_lookup_name(link->id, &namespaceName, &identifier);
            s32 externIdentifier;
            const char *externNamespaceName;
            reasset_id_lookup_name(link->externID, &externNamespaceName, &externIdentifier);
            reasset_log_warning("[reasset] WARN: Unable to resolve %s link %s:%d. External ID %s:%d was not defined.\n",
                data->assetTypeName,
                namespaceName, identifier,
                externNamespaceName, externIdentifier);
            continue;
        }

        resolve_id_internal(data, link->id, resolvedExtern->owner, resolvedExtern->identifier, resolvedExtern->ptr);
    }

    list_free(&data->linkList);
    recomputil_destroy_u32_value_hashmap(data->linkMap);

    data->finalized = TRUE;
}

static void resolve_id_internal(ReAssetResolveMapData *data, ReAssetID id, ReAssetNamespace owner, s32 resolvedIdentifier, void *resolvedPtr) {
    recomputil_u32_memory_hashmap_create(data->idMap, id);
    
    ReAssetResolvedID *resolved = recomputil_u32_memory_hashmap_get(data->idMap, id);
    reasset_assert(resolved != NULL, "[reasset] bug! resolve_id_internal memory hashmap get failed.");
    
    resolved->identifier = resolvedIdentifier;
    resolved->ptr = resolvedPtr;
    resolved->owner = owner;

    recomputil_u32_value_hashmap_insert(data->ownershipMap, resolvedIdentifier, owner);

    // TODO: skip if logging is disabled
    ReAssetIDData *idData = reasset_id_lookup_or_null(id);
    if (idData != NULL && idData->namespace != REASSET_BASE_NAMESPACE) {
        s32 identifier;
        const char *namespaceName;
        reasset_id_lookup_name(id, &namespaceName, &identifier);
        const char *ownerName;
        reasset_namespace_lookup_name(owner, &ownerName);
        reasset_log("[reasset] Resolved %s %s:%d -> %d (owned by %s)\n", 
            data->assetTypeName,
            namespaceName, identifier,
            resolvedIdentifier,
            ownerName);
    }
}

void reasset_resolve_map_resolve_id(ReAssetResolveMap map, ReAssetID id, ReAssetNamespace owner, s32 resolvedIdentifier, void *resolvedPtr) {
    ReAssetResolveMapData *data;
    reasset_assert(recomputil_memory_slotmap_get(sResolveMapSlotmap, map, (void**)&data), 
        "[reasset] Invalid resolve map %d given during resolve_id.", map);

    resolve_id_internal(data, id, owner, resolvedIdentifier, resolvedPtr);
}

RECOMP_EXPORT s32 reasset_resolve_map_lookup(ReAssetResolveMap map, ReAssetID id) {
    ReAssetResolveMapData *data;
    if (recomputil_memory_slotmap_get(sResolveMapSlotmap, map, (void**)&data)) {
        ReAssetResolvedID *resolved = recomputil_u32_memory_hashmap_get(data->idMap, id);
        if (resolved != NULL) {
            return resolved->identifier;
        }
    }

    return -1;
}

RECOMP_EXPORT s32 reasset_resolve_map_lookup_ptr(ReAssetResolveMap map, ReAssetID id, void **outResolvedPtr) {
    reasset_assert(reassetStage == REASSET_STAGE_RESOLVE, 
        "[reasset] reasset_resolve_map_lookup_ptr can only be called during the resolve stage.");

    ReAssetResolveMapData *data;
    if (recomputil_memory_slotmap_get(sResolveMapSlotmap, map, (void**)&data)) {
        ReAssetResolvedID *resolved = recomputil_u32_memory_hashmap_get(data->idMap, id);
        if (resolved != NULL) {
            if (outResolvedPtr != NULL) {
                *outResolvedPtr = resolved->ptr;
            }
            return resolved->identifier;
        }
    }

    return -1;
}

RECOMP_EXPORT ReAssetNamespace reasset_resolve_map_owner_of(ReAssetResolveMap map, s32 resolvedIdentifier) {
    ReAssetResolveMapData *data;
    if (recomputil_memory_slotmap_get(sResolveMapSlotmap, map, (void**)&data)) {
        ReAssetNamespace owner;
        if (recomputil_u32_value_hashmap_get(data->ownershipMap, resolvedIdentifier, &owner)) {
            return owner;
        }
    }

    return REASSET_INVALID_NAMESPACE;
}

void reasset_resolve_map_link(ReAssetResolveMap map, ReAssetID id, ReAssetID externID) {
    ReAssetResolveMapData *data;
    reasset_assert(recomputil_memory_slotmap_get(sResolveMapSlotmap, map, (void**)&data), 
        "[reasset] Invalid resolve map %d given during link.", map);

    ReAssetIDData *idData = reasset_id_lookup(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        s32 identifier;
        const char *namespaceName;
        reasset_id_lookup_name(id, &namespaceName, &identifier);

        reasset_error("[reasset] Creating links in the base namespace is not allowed! Attempted link: %s %s:%d\n",
            data->assetTypeName,
            namespaceName, identifier);
    }

    u32 index;
    if (!recomputil_u32_value_hashmap_get(data->linkMap, id, &index)) {
        index = list_get_length(&data->linkList);
        ReAssetIDLink *link = list_add(&data->linkList);
        link->id = id;
        link->externID = externID;
        recomputil_u32_value_hashmap_insert(data->linkMap, id, index);
    } else {
        ReAssetIDLink *link = list_get(&data->linkList, index);
        if (link->externID != externID) {
            s32 identifier;
            const char *namespaceName;
            reasset_id_lookup_name(id, &namespaceName, &identifier);
            s32 externOldIdentifier;
            const char *externOldNamespaceName;
            reasset_id_lookup_name(link->externID, &externOldNamespaceName, &externOldIdentifier);
            s32 externNewIdentifier;
            const char *externNewNamespaceName;
            reasset_id_lookup_name(externID, &externNewNamespaceName, &externNewIdentifier);
            
            reasset_log_warning("[reasset] WARN: Changing %s link %s:%d: %s:%d -> %s:%d\n",
                data->assetTypeName,
                namespaceName, identifier,
                externOldNamespaceName, externOldIdentifier,
                externNewNamespaceName, externNewIdentifier);
        }

        link->externID = externID;
    }
}
