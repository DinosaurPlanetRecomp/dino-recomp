#include "reasset_mpeg.h"

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

#include "PR/ultratypes.h"
#include "sys/fs.h"
#include "sys/memory.h"

typedef struct {
    ReAssetID id;
    Buffer mpeg;
} MPEGEntry;

static s32 mpegOriginalCount;
static s32 *mpegOriginalTab;
static List mpegList; // list[MPEGEntry]
static U32List mpegIDList; // list[ReAssetID]
static U32ValueHashmapHandle mpegMap; // ReAssetID -> mpeg list index
static ReAssetResolveMap mpegResolveMap;

static MPEGEntry* get_mpeg(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(mpegMap, id, &listIdx)) {
        return list_get(&mpegList, listIdx);
    }

    return NULL;
}

static MPEGEntry* get_or_create_mpeg(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(mpegMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < mpegOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base MPEG: %d", idData->identifier);
        }

        u32list_add(&mpegIDList, id);

        listIdx = list_get_length(&mpegList);
        
        MPEGEntry *entry = list_add(&mpegList);
        entry->id = id;
        buffer_init(&entry->mpeg, 0);

        recomputil_u32_value_hashmap_insert(mpegMap, id, listIdx);
    }

    return list_get(&mpegList, listIdx);
}

static void mpeg_list_element_free(void *element) {
    MPEGEntry *entry = element;
    buffer_free(&entry->mpeg);
}

void reasset_mpeg_init(void) {
    mpegOriginalCount = (reasset_fst_get_file_size(MPEG_TAB) / sizeof(s32)) - 1;
    mpegOriginalTab = reasset_fst_alloc_load_file(MPEG_TAB, NULL);

    list_init(&mpegList, sizeof(MPEGEntry), mpegOriginalCount);
    u32list_init(&mpegIDList, mpegOriginalCount);
    list_set_element_free_callback(&mpegList, mpeg_list_element_free);
    mpegMap = recomputil_create_u32_value_hashmap();
    mpegResolveMap = reasset_resolve_map_create("MPEG");

    // Add base mpeg (preserving order)
    for (s32 i = 0; i < mpegOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        MPEGEntry *entry = get_or_create_mpeg(id);

        s32 offset = mpegOriginalTab[i];
        s32 size = mpegOriginalTab[i + 1] - offset;

        buffer_set_base(&entry->mpeg, MPEG_BIN, offset, size);
    }
}

void reasset_mpeg_repack(void) {
    s32 newCount = list_get_length(&mpegList);

    // Calculate new bin size
    u32 newBinSize = 0;
    for (s32 i = 0; i < newCount; i++) {
        MPEGEntry *entry = list_get(&mpegList, i);
        
        newBinSize += buffer_get_size(&entry->mpeg);
        newBinSize = mmAlign4(newBinSize);
    }

    // Alloc new MPEG.tab/bin
    u32 newTabSize = (newCount + 1) * sizeof(s32);
    s32 *newTab = recomp_alloc(newTabSize);
    void *newBin = recomp_alloc(newBinSize);
    bzero(newBin, newBinSize);

    // Rebuild
    s32 offset = 0;
    for (s32 i = 0; i < newCount; i++) {
        MPEGEntry *entry = list_get(&mpegList, i);
        ReAssetIDData *idData = reasset_id_lookup_data(entry->id);

        if (idData->namespace != REASSET_BASE_NAMESPACE) {
            const char *namespaceName;
            reasset_namespace_lookup_name(idData->namespace, &namespaceName);
            reasset_log("[reasset] New MPEG: %s:%d\n", 
                namespaceName, idData->identifier);
        }

        reasset_resolve_map_resolve_id(mpegResolveMap, entry->id, -1, i, (u8*)newBin + offset);

        newTab[i] = offset;
        buffer_copy_to(&entry->mpeg, newBin, offset);
        offset += buffer_get_size(&entry->mpeg);

        offset = mmAlign4(offset);
    }

    newTab[newCount] = offset;

    // Finalize resolve map
    reasset_resolve_map_finalize(mpegResolveMap);

    // Set new files
    reasset_fst_set_internal(MPEG_TAB, newTab, newTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(MPEG_BIN, newBin, newBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt MPEG.tab & MPEG.bin (count: %d, bin size: 0x%X).\n", newCount, newBinSize);

    // Clean up
    list_free(&mpegList);
    u32list_free(&mpegIDList);
    recomputil_destroy_u32_value_hashmap(mpegMap);
    recomp_free(mpegOriginalTab);
    mpegOriginalTab = NULL;
}

static void assert_custom_mpeg_id(const char *funcName, ReAssetID id) {
    ReAssetIDData *idData = reasset_id_lookup_data(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        return;
    }

    if (idData->identifier >= 0 && idData->identifier <= mpegOriginalCount) {
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_error("[reasset:%s] Custom MPEG identifier %s:%d cannot overlap base MPEG IDs. Reserved IDs: 0-%d.",
            funcName,
            namespaceName, idData->identifier, mpegOriginalCount);
    }
}

RECOMP_EXPORT void reasset_mpeg_set(ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_mpeg_set");
    assert_custom_mpeg_id("reasset_mpeg_set", id);

    MPEGEntry *entry = get_or_create_mpeg(id);
    buffer_set(&entry->mpeg, data, sizeBytes);

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] MPEG set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT void* reasset_mpeg_get(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_mpeg_get");

    MPEGEntry *entry = get_mpeg(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    return buffer_get(&entry->mpeg, outSizeBytes);
}

RECOMP_EXPORT ReAssetIterator reasset_mpeg_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_mpeg_create_iterator");

    return reasset_iterator_create(&mpegIDList);
}

RECOMP_EXPORT void reasset_mpeg_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_mpeg_link");
    assert_custom_mpeg_id("reasset_mpeg_link", id);

    reasset_resolve_map_link(mpegResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_mpeg_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_mpeg_get_resolve_map");

    return mpegResolveMap;
}
