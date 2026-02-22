#include "reasset_music_actions.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/list.h"

#include "PR/ultratypes.h"
#include "dlls/engine/5_amseq.h"
#include "sys/fs.h"

typedef struct {
    ReAssetID id;
    ReAssetIDData *idData;
    s32 resolved;
    MusicAction *action;
} MusicActionPatch;

static s32 mActionOriginalCount;
static List mActionPatches; // list[MusicActionPatch]
static U32ValueHashmapHandle mActionPatchMap; // ReAssetID -> patch list index
static ReAssetResolveMap mActionResolveMap;

static void maction_list_element_free(void *element) {
    MusicActionPatch *patch = element;
    if (patch->action != NULL) {
        recomp_free(patch->action);
        patch->action = NULL;
    }
}

void reasset_music_actions_init(void) {
    mActionOriginalCount = reasset_fst_get_file_size(MUSICACTIONS_BIN) / sizeof(MusicAction);

    list_init(&mActionPatches, sizeof(MusicActionPatch), 0);
    list_set_element_free_callback(&mActionPatches, maction_list_element_free);
    mActionPatchMap = recomputil_create_u32_value_hashmap();
    mActionResolveMap = reasset_resolve_map_create("MusicAction");
}

void reasset_music_actions_repack(void) {
    s32 numPatches = list_get_length(&mActionPatches);

    // Calculate number of new actions and assign IDs for custom assets
    s32 newCount = mActionOriginalCount;
    for (s32 i = 0; i < numPatches; i++) {
        MusicActionPatch *patch = list_get(&mActionPatches, i);
        if (patch->action != NULL && patch->idData->namespace != REASSET_BASE_NAMESPACE) {
            patch->resolved = newCount;
            newCount++;
        }
    }

    // Alloc new MUSICACTIONS.bin
    void *newActions = recomp_alloc(newCount * sizeof(MusicAction));

    // Load original actions
    u32 originalBinSize = mActionOriginalCount * sizeof(MusicAction);
    reasset_fst_read_from_file(MUSICACTIONS_BIN, newActions, 0, originalBinSize);
    // Zero out memory for new actions
    if (newCount > mActionOriginalCount) {
        u32 addedSize = (newCount - mActionOriginalCount) * sizeof(MusicAction);
        bzero((u8*)newActions + originalBinSize, addedSize);
    }

    // Resolve base
    for (s32 i = 0; i < mActionOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        void *maction = (u8*)newActions + (i * sizeof(MusicAction));
        reasset_resolve_map_resolve_id(mActionResolveMap, id, i, maction);
    }

    // Update with patches and resolve custom
    for (s32 i = 0; i < numPatches; i++) {
        MusicActionPatch *patch = list_get(&mActionPatches, i);
        if (patch->action == NULL) {
            continue;
        }

        s32 idx = patch->idData->namespace == REASSET_BASE_NAMESPACE
            ? patch->idData->identifier 
            : patch->resolved;
        
        void *maction = (u8*)newActions + (idx * sizeof(MusicAction));
        bcopy(patch->action, maction, sizeof(MusicAction));

        if (patch->idData->namespace != REASSET_BASE_NAMESPACE) {
            reasset_resolve_map_resolve_id(mActionResolveMap, patch->id, idx, maction);
        }
    }

    // Finalize resolve map
    reasset_resolve_map_finalize(mActionResolveMap);

    // Set new file
    reasset_fst_set_internal(MUSICACTIONS_BIN, newActions, newCount * sizeof(MusicAction), /*ownedByReAsset=*/TRUE);

    reasset_log("[reasset] Rebuilt MUSICACTIONS.bin.\n");

    // Clean up
    list_free(&mActionPatches);
    recomputil_destroy_u32_value_hashmap(mActionPatchMap);
}

static MusicActionPatch *get_maction_patch(ReAssetID id, ReAssetIDData *idData) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(mActionPatchMap, id, &listIdx)) {
        MusicActionPatch patch = { .id = id, .idData = idData, .action = NULL };
        listIdx = list_add(&mActionPatches, &patch);
        recomputil_u32_value_hashmap_insert(mActionPatchMap, id, listIdx);

        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);

        reasset_log("[reasset] Music action patch: %s:%d\n", namespaceName, idData->identifier);
    }

    return list_get(&mActionPatches, listIdx);
}

static void load_maction(ReAssetIDData *idData, MusicActionPatch *patch) {
    if (patch->action == NULL) {
        void *maction = recomp_alloc(sizeof(MusicAction));
        patch->action = maction;

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_fst_read_from_file(MUSICACTIONS_BIN, maction, 
                idData->identifier * sizeof(MusicAction), sizeof(MusicAction));
        } else {
            bzero(maction, sizeof(MusicAction));
        }
    }
}

RECOMP_EXPORT void reasset_music_actions_set(ReAssetID id, const void *data) {
    reasset_assert_stage_set_call("reasset_music_actions_set");

    ReAssetIDData *idData = reasset_id_lookup(id);
    MusicActionPatch *patch = get_maction_patch(id, idData);

    if (patch->action == NULL) {
        patch->action = recomp_alloc(sizeof(MusicAction));
    }

    bcopy(data, patch->action, sizeof(MusicAction));
}

RECOMP_EXPORT void* reasset_music_actions_get(ReAssetID id) {
    reasset_assert_stage_get_call("reasset_music_actions_get");

    ReAssetIDData *idData = reasset_id_lookup(id);
    MusicActionPatch *patch = get_maction_patch(id, idData);
    load_maction(idData, patch);

    return patch->action;
}

RECOMP_EXPORT void reasset_music_actions_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_music_actions_link");

    reasset_resolve_map_link(mActionResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_music_actions_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_music_actions_get_resolve_map");

    return mActionResolveMap;
}
