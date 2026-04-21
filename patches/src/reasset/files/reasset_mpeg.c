#include "reasset_mpeg.h"

#include "patches.h"
#include "recompdata.h"
#include "recomp_funcs.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_iterator.h"
#include "reasset/bin_ptr.h"
#include "reasset/buffer.h"
#include "reasset/list.h"

#include "PR/ultratypes.h"
#include "sys/fs.h"
#include "sys/memory.h"

typedef struct {
    ReAssetID id;
    Buffer mpeg;
    BinPtr mpegPtr;
} MPEGEntry;

static s32 mpegOriginalCount;
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
    s32 *mpegOriginalTab = reasset_fst_alloc_load_file(MPEG_TAB, NULL);

    list_init(&mpegList, sizeof(MPEGEntry), mpegOriginalCount);
    list_set_element_free_callback(&mpegList, mpeg_list_element_free);
    u32list_init(&mpegIDList, mpegOriginalCount);
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

    recomp_free(mpegOriginalTab);
}

void reasset_mpeg_repack(void) {
    u32 startTimeUs = recomp_time_us();

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
            reasset_log_debug("[reasset] New MPEG: %s:%d\n", 
                namespaceName, idData->identifier);
        }


        reasset_resolve_map_resolve_id(mpegResolveMap, entry->id, -1, i);

        newTab[i] = offset;
        bin_ptr_set(&entry->mpegPtr, newBin, offset, buffer_get_size(&entry->mpeg));
        buffer_copy_to_bin_ptr(&entry->mpeg, &entry->mpegPtr);

        offset += buffer_get_size(&entry->mpeg);
        offset = mmAlign4(offset);
    }

    newTab[newCount] = offset;

    // Finalize resolve map
    reasset_resolve_map_finalize(mpegResolveMap);

    // Set new files
    reasset_fst_set_internal(MPEG_TAB, newTab, newTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(MPEG_BIN, newBin, newBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log_info("[reasset] Rebuilt MPEG.tab & MPEG.bin (count: %d, bin size: 0x%X).\n", newCount, newBinSize);

    reasset_log_info("[reasset] MPEG repack completed in %u ms.\n", (recomp_time_us() - startTimeUs) / 1000);
}

void reasset_mpeg_cleanup(void) {
    list_free(&mpegList);
    u32list_free(&mpegIDList);
    recomputil_destroy_u32_value_hashmap(mpegMap);
}

RECOMP_EXPORT void reasset_mpeg_set(ReAssetID id, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_mpeg_set");

    MPEGEntry *entry = get_or_create_mpeg(id);
    buffer_set(&entry->mpeg, data, sizeBytes);

    if (reasset_is_debug_logging_enabled()) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_log_debug("[reasset] MPEG set: %s:%d\n", namespaceName, idData->identifier);
    }
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

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->mpegPtr, NULL);   
    } else {
        return buffer_get(&entry->mpeg, outSizeBytes);
    }
}

RECOMP_EXPORT ReAssetIterator reasset_mpeg_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_mpeg_create_iterator");

    return reasset_iterator_create(&mpegIDList);
}

RECOMP_EXPORT void reasset_mpeg_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_mpeg_link");

    reasset_resolve_map_link(mpegResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_mpeg_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_mpeg_get_resolve_map");

    return mpegResolveMap;
}
