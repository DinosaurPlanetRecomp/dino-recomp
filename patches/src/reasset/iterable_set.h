#pragma once

#include "recompdata.h"

#include "reasset/list.h"

typedef struct {
    List list;
    U32ValueHashmapHandle map; // key -> list idx
} IterableSet;

void iterable_set_init(IterableSet *set, s32 elementSize);
void iterable_set_free(IterableSet *set);
void iterable_set_add(IterableSet *set, u32 key, const void *value);
List* iterable_set_get_list(IterableSet *set);
