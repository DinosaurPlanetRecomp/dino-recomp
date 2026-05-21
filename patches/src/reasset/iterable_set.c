#include "patches.h"
#include "recompdata.h"

#include "reasset/iterable_set.h"
#include "reasset/list.h"

#include "PR/os.h"

// TODO: rename to IterableMap

void iterable_set_init(IterableSet *set, s32 elementSize) {
    bzero(set, sizeof(IterableSet));
    list_init(&set->list, elementSize, 0);
    set->map = recomputil_create_u32_value_hashmap();
}

void iterable_set_free(IterableSet *set) {
    list_free(&set->list);
    recomputil_destroy_u32_value_hashmap(set->map);
}

void iterable_set_add(IterableSet *set, u32 key, const void *value) {
    if (!recomputil_u32_value_hashmap_contains(set->map, key)) {
        u32 listIdx = list_add_copy(&set->list, value);

        recomputil_u32_value_hashmap_insert(set->map, key, listIdx);
    }
}

_Bool iterable_set_contains(IterableSet *set, u32 key) {
    return recomputil_u32_value_hashmap_contains(set->map, key);
}

_Bool iterable_set_get(IterableSet *set, u32 key, void **outValue) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(set->map, key, &listIdx)) {
        void *value = list_get(&set->list, listIdx);
        if (outValue != NULL) {
            *outValue = value;
        }
        return TRUE;
    }

    return FALSE;
}

List* iterable_set_get_list(IterableSet *set) {
    return &set->list;
}
