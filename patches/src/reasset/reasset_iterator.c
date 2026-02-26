#include "reasset_iterator.h"

#include "patches.h"
#include "recompdata.h"

#include "reasset.h"
#include "reasset_id.h"
#include "list.h"

#include "PR/ultratypes.h"

typedef struct {
    s32 nextIndex;
    U32List *list;
} ReAssetIteratorData;

static MemorySlotmapHandle sIteratorSlotmap;

void reasset_iterator_init(void) {
    sIteratorSlotmap = recomputil_create_memory_slotmap(sizeof(ReAssetIteratorData));
}

void reasset_iterator_clear_all(void) {
    recomputil_destroy_memory_slotmap(sIteratorSlotmap);
    sIteratorSlotmap = recomputil_create_memory_slotmap(sizeof(ReAssetIteratorData));
}

ReAssetIterator reasset_iterator_create(U32List *list) {
    ReAssetIterator iterator = recomputil_memory_slotmap_create(sIteratorSlotmap);
    
    ReAssetIteratorData *data;
    reasset_assert(recomputil_memory_slotmap_get(sIteratorSlotmap, iterator, (void**)&data), 
        "[reasset] bug! reasset_iterator_create slotmap get failed.");

    data->list = list;
    data->nextIndex = 0;

    return iterator;
}

RECOMP_EXPORT _Bool reasset_iterator_destroy(ReAssetIterator iterator) {
    return recomputil_memory_slotmap_erase(sIteratorSlotmap, iterator) ? 1 : 0;
}

RECOMP_EXPORT _Bool reasset_iterator_next(ReAssetIterator iterator, ReAssetID *outID) {
    ReAssetIteratorData *data;
    if (!recomputil_memory_slotmap_get(sIteratorSlotmap, iterator, (void**)&data)) {
        reasset_error("[reasset:reasset_iterator_next] Invalid iterator handle: %d", iterator);
        return FALSE;
    }

    if (data->list == NULL) {
        return FALSE;
    }

    s32 length = u32list_get_length(data->list);
    if (data->nextIndex >= length) {
        return FALSE;
    }

    if (outID != NULL) {
        *outID = u32list_get(data->list, data->nextIndex);
    }

    data->nextIndex += 1;

    return TRUE;
}
