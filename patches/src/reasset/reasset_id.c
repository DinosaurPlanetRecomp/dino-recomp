#include "reasset_id.h"

#include "patches.h"
#include "recompdata.h"

#include "reasset.h"
#include "reasset/reasset_namespace.h"

#include "PR/ultratypes.h"

static MemorySlotmapHandle sIDSlotmap; // dict[id, idData]
static U32ValueHashmapHandle sBaseIDHashmap; // dict[s32, id]
static U32ValueHashmapHandle sNamespacedIDHashmap; // dict[namespace, dict[s32, id]]

void reasset_id_init(void) {
    sIDSlotmap = recomputil_create_memory_slotmap(sizeof(ReAssetIDData));
    sBaseIDHashmap = recomputil_create_u32_value_hashmap();
    sNamespacedIDHashmap = recomputil_create_u32_value_hashmap();
}

static ReAssetID id_new(ReAssetIDData **outData) {
    ReAssetID id = recomputil_memory_slotmap_create(sIDSlotmap);
    reasset_assert(recomputil_memory_slotmap_get(sIDSlotmap, id, (void**)outData),
        "[reasset] bug! id_new slotmap get failed.");

    return id;
}

ReAssetIDData *reasset_id_lookup(ReAssetID id) {
    // TODO: better error message? this can happen if mods provide an ID not provided by the api
    ReAssetIDData *data;
    reasset_assert(recomputil_memory_slotmap_get(sIDSlotmap, id, (void**)&data), 
        "[reasset] bug! reasset_id_lookup id lookup failed.");

    return data;
}

ReAssetIDData *reasset_id_lookup_or_null(ReAssetID id) {
    ReAssetIDData *data;
    if (recomputil_memory_slotmap_get(sIDSlotmap, id, (void**)&data)) {
        return data;
    } else {
        return NULL;
    }
}

_Bool reasset_id_lookup_name(ReAssetID id, const char **outNamespaceName, s32 *identifier) {
    ReAssetIDData *data;
    if (recomputil_memory_slotmap_get(sIDSlotmap, id, (void**)&data)) {
        if (identifier != NULL) {
            *identifier = data->identifier;
        }

        return reasset_namespace_lookup_name(data->namespace, outNamespaceName);
    } else {
        if (identifier != NULL) {
            *identifier = -1;
        }
        if (outNamespaceName != NULL) {
            *outNamespaceName = "<NULL>";
        }
        return FALSE;
    }
}

RECOMP_EXPORT ReAssetID reasset_id(ReAssetNamespace namespace, s32 identifier) {
    U32ValueHashmapHandle idMap;
    if (!recomputil_u32_value_hashmap_get(sNamespacedIDHashmap, namespace, &idMap)) {
        idMap = recomputil_create_u32_value_hashmap();
        recomputil_u32_value_hashmap_insert(sNamespacedIDHashmap, namespace, idMap);
    }

    ReAssetID id;
    ReAssetIDData *data;
    if (!recomputil_u32_value_hashmap_get(idMap, identifier, &id)) {
        // Not in map, create
        id = id_new(&data);
        data->namespace = namespace;
        data->identifier = identifier;
        recomputil_u32_value_hashmap_insert(idMap, identifier, id);
    }

    return id;
}

RECOMP_EXPORT ReAssetID reasset_base_id(s32 identifier) {
    ReAssetID id;
    ReAssetIDData *data;
    if (!recomputil_u32_value_hashmap_get(sBaseIDHashmap, identifier, &id)) {
        // Not in map, create
        id = id_new(&data);
        data->namespace = 0;
        data->identifier = identifier;
        recomputil_u32_value_hashmap_insert(sBaseIDHashmap, identifier, id);
    }

    return id;
}
