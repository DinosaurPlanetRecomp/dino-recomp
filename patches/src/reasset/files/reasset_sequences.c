#include "reasset_sequences.h"

#include "patches.h"
#include "recompdata.h"
#include "recomp_funcs.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_iterator.h"
#include "reasset/files/reasset_maps.h"
#include "reasset/files/reasset_objects.h"
#include "reasset/buffer.h"
#include "reasset/list.h"
#include "reasset/bin_ptr.h"

#include "PR/ultratypes.h"
#include "libc/string.h"
#include "sys/fs.h"
#include "sys/memory.h"

typedef struct {
    u32 uid;
    u8 settings;
    s16 objectID;
} Actor;

/// ANIMCURVES.tab entry
typedef struct {
    u16 size;
    u16 eventCount;
    u32 offset;
} AnimCurveHeader;

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    s32 eventCount;
    Buffer curve;
    BinPtr curvePtr;
} AnimCurveEntry;

typedef struct {
    s32 eventCount;
    Buffer curve;
    BinPtr curvePtr;
} ActorCurve;

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    ReAssetID mapID;
    _Bool hasMap;
    Buffer seq;
    BinPtr seqPtr;
    List actorCurves; // list[ActorCurve]
} ObjSeqEntry;

static s32 curvesOriginalCount;
static List curvesList; // list[AnimCurveEntry]
static U32List curvesIDList; // list[ReAssetID]
static U32ValueHashmapHandle curvesMap; // ReAssetID -> curves list index
static ReAssetResolveMap curvesResolveMap;

static s32 seqsOriginalCount;
static List seqsList; // list[ObjSeqEntry]
static U32List seqsIDList; // list[ReAssetID]
static U32ValueHashmapHandle seqsMap; // ReAssetID -> seqs list index
static ReAssetResolveMap seqsResolveMap;
static U32ValueHashmapHandle actorCurveMapMap; // ReAssetID (seq) -> U32ValueHashmapHandle (actorIdx -> curveTabIdx)

static void actor_curves_list_element_free(void *element) {
    ActorCurve *entry = element;
    buffer_free(&entry->curve);
}

static void curves_list_element_free(void *element) {
    AnimCurveEntry *entry = element;
    buffer_free(&entry->curve);
}

static void seqs_list_element_free(void *element) {
    ObjSeqEntry *entry = element;
    buffer_free(&entry->seq);
    list_free(&entry->actorCurves);
}

static ActorCurve* get_actor_curve(ObjSeqEntry *entry, s32 actor) {
    if (actor < list_get_length(&entry->actorCurves)) {
        return list_get(&entry->actorCurves, actor);
    }

    return NULL;
}

static ActorCurve* get_or_create_actor_curve(ObjSeqEntry *entry, s32 actor) {
    s32 listLen = list_get_length(&entry->actorCurves);
    if (actor >= listLen) {
        list_set_length(&entry->actorCurves, actor + 1);
        // Init new buffers from the list resize
        for (s32 i = listLen; i < actor + 1; i++) {
            ActorCurve *curve = list_get(&entry->actorCurves, i);
            buffer_init(&curve->curve, 0);
            curve->eventCount = 0;
        }
    }

    return list_get(&entry->actorCurves, actor);
}

static AnimCurveEntry* get_curve(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(curvesMap, id, &listIdx)) {
        return list_get(&curvesList, listIdx);
    }

    return NULL;
}

static AnimCurveEntry* get_or_create_curve(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(curvesMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < curvesOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base anim curve: %d", idData->identifier);
        }

        u32list_add(&curvesIDList, id);

        listIdx = list_get_length(&curvesList);
        
        AnimCurveEntry *entry = list_add(&curvesList);
        entry->id = id;
        entry->owner = idData->namespace;
        buffer_init(&entry->curve, 0);

        recomputil_u32_value_hashmap_insert(curvesMap, id, listIdx);
    }

    return list_get(&curvesList, listIdx);
}

static ObjSeqEntry* get_seq(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(seqsMap, id, &listIdx)) {
        return list_get(&seqsList, listIdx);
    }

    return NULL;
}

static ObjSeqEntry* get_or_create_seq(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(seqsMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < seqsOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base object sequence: %d", idData->identifier);
        }

        u32list_add(&seqsIDList, id);

        listIdx = list_get_length(&seqsList);
        
        ObjSeqEntry *entry = list_add(&seqsList);
        entry->id = id;
        entry->owner = idData->namespace;
        buffer_init(&entry->seq, 0);
        list_init(&entry->actorCurves, sizeof(ActorCurve), 2);
        list_set_element_free_callback(&entry->actorCurves, actor_curves_list_element_free);

        recomputil_u32_value_hashmap_insert(seqsMap, id, listIdx);

        // Create map for actor curves
        recomputil_u32_value_hashmap_insert(actorCurveMapMap, id, recomputil_create_u32_value_hashmap());
    }

    return list_get(&seqsList, listIdx);
}

void reasset_sequences_init(void) {
    // ObjSeq2Curve
    u16 *seq2curveTab = reasset_fst_alloc_load_file(OBJSEQ2CURVE_TAB, NULL);
    s32 firstSeqCurve = seq2curveTab[0];

    // Anim Curves
    curvesOriginalCount = (reasset_fst_get_file_size(ANIMCURVES_TAB) / sizeof(AnimCurveHeader)) - 1;
    AnimCurveHeader *curvesOriginalTab = reasset_fst_alloc_load_file(ANIMCURVES_TAB, NULL);

    if (firstSeqCurve < curvesOriginalCount) {
        // We're only tracking curves that aren't associated with a seq as independent curves
        curvesOriginalCount = firstSeqCurve;
    }

    list_init(&curvesList, sizeof(AnimCurveEntry), curvesOriginalCount);
    list_set_element_free_callback(&curvesList, curves_list_element_free);
    u32list_init(&curvesIDList, curvesOriginalCount);
    curvesMap = recomputil_create_u32_value_hashmap();
    curvesResolveMap = reasset_resolve_map_create("AnimCurve");

    // Add base anim curves (preserving order)
    for (s32 i = 0; i < curvesOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        AnimCurveEntry *entry = get_or_create_curve(id);

        AnimCurveHeader *header = &curvesOriginalTab[i];

        s32 offset = header->offset;
        s32 size = header->size;

        entry->eventCount = header->eventCount;

        buffer_set_base(&entry->curve, ANIMCURVES_BIN, offset, size);
    }

    // Object Sequences
    seqsOriginalCount = (reasset_fst_get_file_size(OBJSEQ_TAB) / sizeof(u16)) - 2;
    u16 *seqsOriginalTab = reasset_fst_alloc_load_file(OBJSEQ_TAB, NULL);

    list_init(&seqsList, sizeof(ObjSeqEntry), seqsOriginalCount);
    list_set_element_free_callback(&seqsList, seqs_list_element_free);
    u32list_init(&seqsIDList, seqsOriginalCount);
    seqsMap = recomputil_create_u32_value_hashmap();
    seqsResolveMap = reasset_resolve_map_create("ObjectSequence");
    actorCurveMapMap = recomputil_create_u32_value_hashmap();

    // Add base obj seqs (preserving order)
    for (s32 i = 0; i < seqsOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        ObjSeqEntry *entry = get_or_create_seq(id);

        s32 startActor = seqsOriginalTab[i];
        s32 numActors = seqsOriginalTab[i + 1] - startActor;
        s32 offset = startActor * 8;
        s32 size = numActors * 8;

        buffer_set_base(&entry->seq, OBJSEQ_BIN, offset, size);

        // Map curves
        s32 curvesStart = seq2curveTab[i];
        s32 curvesEnd = seq2curveTab[i + 1];

        for (s32 curveIdx = curvesStart, actor = 0; curveIdx < curvesEnd; curveIdx++, actor++) {
            ActorCurve *curve = get_or_create_actor_curve(entry, actor);
            
            AnimCurveHeader *header = &curvesOriginalTab[curveIdx];
            s32 curveOffset = header->offset;
            s32 curveSize = header->size;

            curve->eventCount = header->eventCount;

            buffer_set_base(&curve->curve, ANIMCURVES_BIN, curveOffset, curveSize);
        }
    }

    // Clean up
    recomp_free(curvesOriginalTab);
    recomp_free(seqsOriginalTab);
    recomp_free(seq2curveTab);
}

void reasset_sequences_repack(void) {
    u32 startTimeUs = recomp_time_us();

    // Calculate sizes
    s32 newCurveCount = list_get_length(&curvesList);
    u32 animCurvesBinSize = 0;
    for (s32 i = 0; i < newCurveCount; i++) {
        AnimCurveEntry *curve = list_get(&curvesList, i);
        animCurvesBinSize += buffer_get_size(&curve->curve);
        animCurvesBinSize = mmAlign4(animCurvesBinSize);
    }

    s32 newSeqCount = list_get_length(&seqsList);
    s32 newSeqCurveCount = 0;
    u32 objSeqBinSize = 0;
    for (s32 i = 0; i < newSeqCount; i++) {
        ObjSeqEntry *seq = list_get(&seqsList, i);
        u32 seqSize = mmAlign8(buffer_get_size(&seq->seq));
        s32 numActors = seqSize / 8;

        // Note: We're writing a curve for each actor regardless of if the curve was defined
        newSeqCurveCount += numActors;
        objSeqBinSize += seqSize;

        s32 numActorCurves = list_get_length(&seq->actorCurves);
        for (s32 k = 0; k < numActorCurves; k++) {
            ActorCurve *curve = list_get(&seq->actorCurves, k);

            animCurvesBinSize += buffer_get_size(&curve->curve);
            animCurvesBinSize = mmAlign4(animCurvesBinSize);
        }
    }

    u32 animCurvesTabSize = (newCurveCount + newSeqCurveCount + 1) * sizeof(AnimCurveHeader);
    u32 objSeqTabSize = (newSeqCount + 2) * sizeof(u16);
    u32 objSeq2CurveTabSize = (newSeqCount + 2) * sizeof(u16);

    // Allocate new container files
    void *animCurvesBin = recomp_alloc(animCurvesBinSize);
    AnimCurveHeader *animCurvesTab = recomp_alloc(animCurvesTabSize);
    void *objSeqBin = recomp_alloc(objSeqBinSize);
    u16 *objSeqTab = recomp_alloc(objSeqTabSize);
    u16 *objSeq2CurveTab = recomp_alloc(objSeq2CurveTabSize);

    // Write standalone curves
    s32 animCurvesBinOffset = 0;
    s32 animCurvesTabIdx = 0;
    for (s32 i = 0; i < newCurveCount; i++) {
        AnimCurveEntry *curve = list_get(&curvesList, i);
        
        AnimCurveHeader *tabEntry = &animCurvesTab[animCurvesTabIdx];
        tabEntry->size = buffer_get_size(&curve->curve);
        tabEntry->eventCount = curve->eventCount;
        tabEntry->offset = animCurvesBinOffset;

        bin_ptr_set(&curve->curvePtr, animCurvesBin, animCurvesBinOffset, tabEntry->size);
        buffer_copy_to_bin_ptr(&curve->curve, &curve->curvePtr);

        reasset_resolve_map_resolve_id(curvesResolveMap, curve->id, curve->owner, animCurvesTabIdx);

        animCurvesTabIdx += 1;

        animCurvesBinOffset += tabEntry->size;
        animCurvesBinOffset = mmAlign4(animCurvesBinOffset);
    }

    // Write object sequences
    s32 objSeqBinOffset = 0;
    s32 objSeqBinActorIdx = 0;
    s32 objSeqTabIdx = 0;
    s32 objSeq2CurveTabIdx = 0;
    for (s32 i = 0; i < newSeqCount; i++) {
        ObjSeqEntry *seq = list_get(&seqsList, i);

        // Write sequence
        u32 seqSize = mmAlign8(buffer_get_size(&seq->seq));
        s32 numActors = seqSize / 8;
        
        objSeqTab[objSeqTabIdx] = objSeqBinActorIdx;
        bin_ptr_set(&seq->seqPtr, objSeqBin, objSeqBinOffset, seqSize);
        buffer_copy_to_bin_ptr(&seq->seq, &seq->seqPtr);

        reasset_resolve_map_resolve_id(seqsResolveMap, seq->id, seq->owner, objSeqTabIdx);

        objSeqBinActorIdx += numActors;

        objSeqTabIdx += 1;

        objSeqBinOffset += seqSize;
        objSeqBinOffset = mmAlign8(objSeqBinOffset);

        // Write Seq2Curve
        objSeq2CurveTab[objSeq2CurveTabIdx] = animCurvesTabIdx;
        objSeq2CurveTabIdx += 1;
        
        // Write curves
        U32ValueHashmapHandle actorCurveMap;
        reasset_assert(recomputil_u32_value_hashmap_get(actorCurveMapMap, seq->id, &actorCurveMap),
            "[reasset] bug! Object sequence %d actor curve map hashmap get failed.", i);
        s32 numActorCurves = list_get_length(&seq->actorCurves);
        for (s32 k = 0; k < numActors; k++) {
            ActorCurve *curve = k < numActorCurves
                ? list_get(&seq->actorCurves, k)
                : NULL;
            AnimCurveHeader *curveTabEntry = &animCurvesTab[animCurvesTabIdx];

            recomputil_u32_value_hashmap_insert(actorCurveMap, k, animCurvesTabIdx);

            if (curve != NULL) {
                curveTabEntry->size = buffer_get_size(&curve->curve);
                curveTabEntry->eventCount = curve->eventCount;
                curveTabEntry->offset = animCurvesBinOffset;

                buffer_copy_to(&curve->curve, animCurvesBin, animCurvesBinOffset);

                animCurvesTabIdx += 1;

                animCurvesBinOffset += curveTabEntry->size;
                animCurvesBinOffset = mmAlign4(animCurvesBinOffset);
            } else {
                // No curve defined, just write a blank one
                curveTabEntry->size = 0;
                curveTabEntry->eventCount = 0;
                curveTabEntry->offset = animCurvesBinOffset;

                animCurvesTabIdx += 1;
            }
        }
    }

    // Terminate files
    animCurvesTab[animCurvesTabIdx].size = -1;
    animCurvesTab[animCurvesTabIdx].eventCount = -1;
    animCurvesTab[animCurvesTabIdx].size = -1;
    
    objSeq2CurveTab[objSeq2CurveTabIdx] = animCurvesTabIdx;
    objSeq2CurveTab[objSeq2CurveTabIdx + 1] = -1;

    objSeqTab[objSeqTabIdx] = objSeqBinActorIdx;
    objSeqTab[objSeqTabIdx + 1] = -1;

    // Finalize resolve maps
    reasset_resolve_map_finalize(curvesResolveMap);
    reasset_resolve_map_finalize(seqsResolveMap);

    // Set new files
    reasset_fst_set_internal(ANIMCURVES_BIN, animCurvesBin, animCurvesBinSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(ANIMCURVES_TAB, animCurvesTab, animCurvesTabSize, /*ownedByReAsset=*/TRUE);
    reasset_log_info("[reasset] Rebuilt ANIMCURVES.tab & ANIMCURVES.bin (count: %d, bin size: 0x%X).\n", newCurveCount + newSeqCurveCount, animCurvesBinSize);
    reasset_fst_set_internal(OBJSEQ_BIN, objSeqBin, objSeqBinSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(OBJSEQ_TAB, objSeqTab, objSeqTabSize, /*ownedByReAsset=*/TRUE);
    reasset_log_info("[reasset] Rebuilt OBJSEQ.tab & OBJSEQ.bin (count: %d, bin size: 0x%X).\n", newSeqCount, objSeqBinSize);
    reasset_fst_set_internal(OBJSEQ2CURVE_TAB, objSeq2CurveTab, objSeq2CurveTabSize, /*ownedByReAsset=*/TRUE);
    reasset_log_info("[reasset] Rebuilt OBJSEQ2CURVE.tab (count: %d).\n", newSeqCount);

    reasset_log_info("[reasset] Sequences repack completed in %u ms.\n", (recomp_time_us() - startTimeUs) / 1000);
}

void reasset_sequences_patch(void) {
    ReAssetResolveMap objIndexResolveMap = reasset_object_indices_get_resolve_map();

    s32 newSeqCount = list_get_length(&seqsList);
    for (s32 i = 0; i < newSeqCount; i++) {
        ObjSeqEntry *seq = list_get(&seqsList, i);
        if (seq->owner == REASSET_BASE_NAMESPACE) {
            // Don't patch assets owned by base
            continue;
        }
        
        // Get pointer to seq data
        s32 numActors = seq->seqPtr.size / sizeof(Actor);
        Actor *actors = seq->seqPtr.ptr;
        if (actors == NULL) {
            continue;
        }

        // If the seq is associated with a map, get its map object resolve map
        ReAssetResolveMap mapObjResolveMap;
        if (seq->hasMap) {
            mapObjResolveMap = reasset_map_objects_get_resolve_map(seq->mapID);
        }

        // Patch actors
        for (s32 actorIdx = 0; actorIdx < numActors; actorIdx++) {
            Actor *actor = &actors[actorIdx];
            
            if (seq->hasMap && actor->uid != 0) {
                // Patch UID
                s32 resolvedUID = reasset_resolve_map_lookup(mapObjResolveMap, reasset_id(seq->owner, actor->uid));
                if (resolvedUID != -1) {
                    actor->uid = resolvedUID;
                } else if (!reasset_map_objects_is_base_uid(seq->mapID, actor->uid)) {
                    const char *namespaceName;
                    s32 identifier;
                    reasset_id_lookup_name(seq->id, &namespaceName, &identifier);
                    reasset_log_warning("[reasset] WARN: Failed to patch sequence (%s:%d) actor %d UID %d. UID was not defined!",
                        namespaceName, identifier, actorIdx, actor->uid);
                    actor->uid = 0;
                }
            }

            if (actor->objectID != 0) {
                // Patch object ID
                s32 resolvedID = reasset_resolve_map_lookup(objIndexResolveMap, reasset_id(seq->owner, actor->objectID));
                if (resolvedID != -1) {
                    actor->objectID = resolvedID;
                } else if (!reasset_object_indices_is_base_id(actor->objectID)) {
                    const char *namespaceName;
                    s32 identifier;
                    reasset_id_lookup_name(seq->id, &namespaceName, &identifier);
                    reasset_log_warning("[reasset] WARN: Failed to patch sequence (%s:%d) actor %d object ID %d. Object index was not defined!",
                        namespaceName, identifier, actorIdx, actor->objectID);
                    actor->objectID = 0;
                }
            }
        }
    }
}

void reasset_sequences_cleanup(void) {
    s32 newSeqCount = list_get_length(&seqsList);
    for (s32 i = 0; i < newSeqCount; i++) {
        ObjSeqEntry *seq = list_get(&seqsList, i);

        U32ValueHashmapHandle actorCurveMap;
        if (recomputil_u32_value_hashmap_get(actorCurveMapMap, seq->id, &actorCurveMap)) {
            recomputil_destroy_u32_value_hashmap(actorCurveMap);
        }
    }

    list_free(&curvesList);
    u32list_free(&curvesIDList);
    recomputil_destroy_u32_value_hashmap(curvesMap);

    list_free(&seqsList);
    u32list_free(&seqsIDList);
    recomputil_destroy_u32_value_hashmap(seqsMap);
    recomputil_destroy_u32_value_hashmap(actorCurveMapMap);
}

// MARK: Anim Curves

RECOMP_EXPORT void reasset_anim_curves_set(ReAssetID id, ReAssetNamespace owner, s32 eventCount, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_anim_curves_set");

    AnimCurveEntry *entry = get_or_create_curve(id);
    buffer_set(&entry->curve, data, sizeBytes);
    entry->owner = owner;
    entry->eventCount = eventCount;

    if (reasset_is_debug_logging_enabled()) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_log_debug("[reasset] Anim curve set: %s:%d\n", namespaceName, idData->identifier);
    }
}

RECOMP_EXPORT void* reasset_anim_curves_get(ReAssetID id, s32 *outEventCount, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_anim_curves_get");

    AnimCurveEntry *entry = get_curve(id);
    if (entry == NULL) {
        if (outEventCount != NULL) {
            *outEventCount = 0;
        }
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (outEventCount != NULL) {
        *outEventCount = entry->eventCount;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->curvePtr, outSizeBytes);
    } else {
        return buffer_get(&entry->curve, outSizeBytes);
    }
}

RECOMP_EXPORT ReAssetIterator reasset_anim_curves_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_anim_curves_create_iterator");

    return reasset_iterator_create(&curvesIDList);
}

RECOMP_EXPORT void reasset_anim_curves_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_anim_curves_link");

    reasset_resolve_map_link(curvesResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_anim_curves_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_anim_curves_get_resolve_map");

    return curvesResolveMap;
}

// MARK: Object Sequences

_Bool reasset_object_sequences_is_base_id(s32 id) {
    return id >= 0 && id < seqsOriginalCount;
}

static void set_seq(ReAssetID id, ReAssetNamespace owner, ReAssetID map, _Bool hasMap, const void *data, u32 sizeBytes) {
    ObjSeqEntry *entry = get_or_create_seq(id);
    buffer_set(&entry->seq, data, sizeBytes);
    entry->owner = owner;
    entry->mapID = map;
    entry->hasMap = hasMap;

    if (reasset_is_debug_logging_enabled()) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_log_debug("[reasset] Object sequence set: %s:%d\n", namespaceName, idData->identifier);
    }
}

RECOMP_EXPORT void reasset_object_sequences_set(ReAssetID id, ReAssetNamespace owner, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_object_sequences_set");

    set_seq(id, owner, -1, FALSE, data, sizeBytes);
}

RECOMP_EXPORT void reasset_object_sequences_set_ex(ReAssetID id, ReAssetNamespace owner, ReAssetID map, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_object_sequences_set_ex");

    set_seq(id, owner, map, TRUE, data, sizeBytes);
}

RECOMP_EXPORT void* reasset_object_sequences_get(ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_object_sequences_get");

    ObjSeqEntry *entry = get_seq(id);
    if (entry == NULL) {
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->seqPtr, outSizeBytes);
    } else {
        return buffer_get(&entry->seq, outSizeBytes);
    }
}

RECOMP_EXPORT void reasset_object_sequences_set_curve(ReAssetID id, s32 actor, s32 eventCount, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_object_sequences_set_curve");

    ObjSeqEntry *seq = get_or_create_seq(id);

    ActorCurve *curve = get_or_create_actor_curve(seq, actor);
    buffer_set(&curve->curve, data, sizeBytes);
    curve->eventCount = eventCount;

    if (reasset_is_debug_logging_enabled()) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_log_debug("[reasset] Object sequence curve set: %s:%d[%d]\n", 
            namespaceName, idData->identifier, actor);
    }
}

RECOMP_EXPORT void* reasset_object_sequences_get_curve(ReAssetID id, s32 actor, s32 *outEventCount, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_object_sequences_get_curve");

    ObjSeqEntry *seq = get_seq(id);
    if (seq == NULL) {
        if (outEventCount != NULL) {
            *outEventCount = 0;
        }
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    ActorCurve *curve = get_actor_curve(seq, actor);
    if (curve == NULL) {
        if (outEventCount != NULL) {
            *outEventCount = 0;
        }
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (outEventCount != NULL) {
        *outEventCount = curve->eventCount;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&curve->curvePtr, outSizeBytes);
    } else {
        return buffer_get(&curve->curve, outSizeBytes);
    }
}

RECOMP_EXPORT ReAssetIterator reasset_object_sequences_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_object_sequences_create_iterator");

    return reasset_iterator_create(&seqsIDList);
}

RECOMP_EXPORT void reasset_object_sequences_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_object_sequences_link");

    reasset_resolve_map_link(seqsResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_object_sequences_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_object_sequences_get_resolve_map");

    return seqsResolveMap;
}
