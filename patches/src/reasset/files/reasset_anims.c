#include "reasset_anims.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_iterator.h"
#include "reasset/buffer.h"
#include "reasset/list.h"
#include "reasset/bin_ptr.h"

#include "PR/ultratypes.h"
#include "sys/fs.h"
#include "sys/memory.h"

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    Buffer anim;
    BinPtr animPtr;
} AnimEntry;

static s32 animOriginalCount;
static List animList; // list[AnimEntry]
static U32List animIDList; // list[ReAssetID]
static U32ValueHashmapHandle animMap; // ReAssetID -> anim list index
static ReAssetResolveMap animResolveMap;

static AnimEntry* get_anim(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(animMap, id, &listIdx)) {
        return list_get(&animList, listIdx);
    }

    return NULL;
}

static AnimEntry* get_or_create_anim(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(animMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < animOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base anim: %d", idData->identifier);
        }

        u32list_add(&animIDList, id);

        listIdx = list_get_length(&animList);
        
        AnimEntry *entry = list_add(&animList);
        entry->id = id;
        entry->owner = idData->namespace;
        buffer_init(&entry->anim, 0);

        recomputil_u32_value_hashmap_insert(animMap, id, listIdx);
    }

    return list_get(&animList, listIdx);
}

static void anim_list_element_free(void *element) {
    AnimEntry *entry = element;
    buffer_free(&entry->anim);
}

void reasset_anims_init(void) {
    animOriginalCount = (reasset_fst_get_file_size(ANIM_TAB) / sizeof(s32)) - 2;
    s32 *originalObjTab = reasset_fst_alloc_load_file(ANIM_TAB, NULL);

    list_init(&animList, sizeof(AnimEntry), animOriginalCount);
    list_set_element_free_callback(&animList, anim_list_element_free);
    u32list_init(&animIDList, animOriginalCount);
    animMap = recomputil_create_u32_value_hashmap();
    animResolveMap = reasset_resolve_map_create("Anim");

    // Add base anims (preserving order)
    for (s32 i = 0; i < animOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        AnimEntry *entry = get_or_create_anim(id);

        s32 offset = originalObjTab[i];
        s32 size = originalObjTab[i + 1] - offset;

        buffer_set_base(&entry->anim, ANIM_BIN, offset, size);
    }

    recomp_free(originalObjTab);
}

void reasset_anims_repack(void) {
    s32 newCount = list_get_length(&animList);

    // Calculate new ANIM.bin size
    u32 newBinSize = 0;
    for (s32 i = 0; i < newCount; i++) {
        AnimEntry *entry = list_get(&animList, i);
        
        newBinSize += buffer_get_size(&entry->anim);
        newBinSize = mmAlign4(newBinSize);
    }

    // Alloc new ANIM.tab/bin
    u32 newTabSize = (newCount + 2) * sizeof(s32);
    s32 *newTab = recomp_alloc(newTabSize);
    void *newBin = recomp_alloc(newBinSize);
    bzero(newBin, newBinSize);

    // Rebuild
    s32 offset = 0;
    for (s32 i = 0; i < newCount; i++) {
        AnimEntry *entry = list_get(&animList, i);
        ReAssetIDData *idData = reasset_id_lookup_data(entry->id);

        if (idData->namespace != REASSET_BASE_NAMESPACE) {
            const char *namespaceName;
            reasset_namespace_lookup_name(idData->namespace, &namespaceName);
            reasset_log_debug("[reasset] New anim: %s:%d\n", 
                namespaceName, idData->identifier);
        }

        reasset_resolve_map_resolve_id(animResolveMap, entry->id, entry->owner, i);

        newTab[i] = offset;
        bin_ptr_set(&entry->animPtr, newBin, offset, buffer_get_size(&entry->anim));
        buffer_copy_to_bin_ptr(&entry->anim, &entry->animPtr);
        offset += buffer_get_size(&entry->anim);

        offset = mmAlign4(offset);
    }

    newTab[newCount] = offset;
    newTab[newCount + 1] = -1;

    // Finalize resolve map
    reasset_resolve_map_finalize(animResolveMap);

    // Set new files
    reasset_fst_set_internal(ANIM_TAB, newTab, newTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(ANIM_BIN, newBin, newBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log_info("[reasset] Rebuilt ANIM.tab & ANIM.bin (count: %d, bin size: 0x%X).\n", newCount, newBinSize);
}

void reasset_anims_cleanup(void) {
    list_free(&animList);
    u32list_free(&animIDList);
    recomputil_destroy_u32_value_hashmap(animMap);
}

RECOMP_EXPORT void reasset_anims_set(ReAssetID id, ReAssetNamespace owner, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_anims_set");

    AnimEntry *entry = get_or_create_anim(id);
    buffer_set(&entry->anim, data, sizeBytes);
    entry->owner = owner;

    if (reasset_is_debug_logging_enabled()) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_log_debug("[reasset] Anim set: %s:%d\n", namespaceName, idData->identifier);
    }
}

RECOMP_EXPORT void* reasset_anims_get(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_anims_get");

    AnimEntry *entry = get_anim(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->animPtr, outSizeBytes);
    } else {
        return buffer_get(&entry->anim, outSizeBytes);
    }
}

RECOMP_EXPORT ReAssetIterator reasset_anims_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_anims_create_iterator");

    return reasset_iterator_create(&animIDList);
}

RECOMP_EXPORT void reasset_anims_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_anims_link");

    reasset_resolve_map_link(animResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_anims_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_anims_get_resolve_map");

    return animResolveMap;
}

_Bool reasset_anims_is_base_id(s32 id) {
    return id >= 0 && id < animOriginalCount;
}
