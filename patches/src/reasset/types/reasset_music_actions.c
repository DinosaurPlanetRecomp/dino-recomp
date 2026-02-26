#include "reasset_music_actions.h"

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
#include "dlls/engine/5_amseq.h"
#include "sys/fs.h"

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    Buffer action;
} MusicActionEntry;

static s32 mActionOriginalCount;
static List mActionList; // list[MusicActionEntry]
static U32List mActionIDList; // list[ReAssetID]
static U32ValueHashmapHandle mActionMap; // ReAssetID -> action list index
static ReAssetResolveMap mActionResolveMap;

static MusicActionEntry* get_maction(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(mActionMap, id, &listIdx)) {
        return list_get(&mActionList, listIdx);
    }

    return NULL;
}

static MusicActionEntry* get_or_create_maction(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(mActionMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < mActionOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base music action: %d", idData->identifier);
        }

        u32list_add(&mActionIDList, id);

        listIdx = list_get_length(&mActionList);
        
        MusicActionEntry *entry = list_add(&mActionList);
        entry->id = id;
        entry->owner = idData->namespace;
        buffer_init(&entry->action, 0);

        recomputil_u32_value_hashmap_insert(mActionMap, id, listIdx);
    }

    return list_get(&mActionList, listIdx);
}

static void maction_list_element_free(void *element) {
    MusicActionEntry *patch = element;
    buffer_free(&patch->action);
}

void reasset_music_actions_init(void) {
    mActionOriginalCount = reasset_fst_get_file_size(MUSICACTIONS_BIN) / sizeof(MusicAction);

    list_init(&mActionList, sizeof(MusicActionEntry), mActionOriginalCount);
    u32list_init(&mActionIDList, mActionOriginalCount);
    list_set_element_free_callback(&mActionList, maction_list_element_free);
    mActionMap = recomputil_create_u32_value_hashmap();
    mActionResolveMap = reasset_resolve_map_create("MusicAction");

    // Add base actions (preserving order)
    for (s32 i = 0; i < mActionOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        MusicActionEntry *entry = get_or_create_maction(id);
        buffer_set_base(&entry->action, MUSICACTIONS_BIN, i * sizeof(MusicAction), sizeof(MusicAction));
    }
}

void reasset_music_actions_repack(void) {
    // Alloc new MUSICACTIONS.bin
    s32 newCount = list_get_length(&mActionList);
    void *newActions = recomp_alloc(newCount * sizeof(MusicAction));

    // Write new actions
    for (s32 i = 0; i < newCount; i++) {
        MusicActionEntry *entry = list_get(&mActionList, i);

        if (buffer_is_set(&entry->action)) {
            ReAssetIDData *idData = reasset_id_lookup_data(entry->id);
            const char *namespaceName;
            reasset_namespace_lookup_name(idData->namespace, &namespaceName);
            if (idData->namespace != REASSET_BASE_NAMESPACE) {
                reasset_log("[reasset] New music action: %s:%d\n", namespaceName, idData->identifier);
            } else {
                reasset_log("[reasset] Music action patch: %s:%d\n", namespaceName, idData->identifier);
            }
        }

        void *dst = (u8*)newActions + (i * sizeof(MusicAction));
        buffer_copy_to(&entry->action, dst, 0);

        reasset_resolve_map_resolve_id(mActionResolveMap, entry->id, entry->owner, i, dst);
    }

    // Finalize resolve map
    reasset_resolve_map_finalize(mActionResolveMap);

    // Set new file
    reasset_fst_set_internal(MUSICACTIONS_BIN, newActions, newCount * sizeof(MusicAction), /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt MUSICACTIONS.bin (length: %d).\n", newCount);

    // Clean up
    u32list_free(&mActionIDList);
    list_free(&mActionList);
    recomputil_destroy_u32_value_hashmap(mActionMap);
}

RECOMP_EXPORT void reasset_music_actions_set(ReAssetID id, ReAssetNamespace owner, const void *data) {
    reasset_assert_stage_set_call("reasset_music_actions_set");

    MusicActionEntry *entry = get_or_create_maction(id);
    buffer_set(&entry->action, data, sizeof(MusicAction));
    entry->owner = owner;
}

RECOMP_EXPORT void* reasset_music_actions_get(ReAssetID id) {
    reasset_assert_stage_get_call("reasset_music_actions_get");

    MusicActionEntry *entry = get_maction(id);
    if (entry == NULL) {
        return NULL;
    }

    return buffer_get(&entry->action, NULL);
}

RECOMP_EXPORT ReAssetIterator reasset_music_actions_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_music_actions_create_iterator");

    return reasset_iterator_create(&mActionIDList);
}

RECOMP_EXPORT void reasset_music_actions_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_music_actions_link");

    reasset_resolve_map_link(mActionResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_music_actions_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_music_actions_get_resolve_map");

    return mActionResolveMap;
}
