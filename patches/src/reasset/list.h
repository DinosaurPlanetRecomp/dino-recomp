#pragma once

#include "PR/ultratypes.h"

typedef void (*ListElementFreeCallback)(void *element);

typedef struct {
    void *data;
    s32 elementSize;
    s32 length;
    s32 capacity;
    ListElementFreeCallback elementFreeCallback;
} List;

void list_init(List *list, s32 elementSize, s32 initialCapacity);
void list_set_element_free_callback(List *list, ListElementFreeCallback callback);
void list_free(List *list);
void list_clear(List *list);
void list_set_capacity(List *list, s32 capacity);
void list_set_length(List *list, s32 length);
s32 list_get_length(List *list);
/** Makes copy of element. Returns index of added element. */
s32 list_add(List *list, const void *element);
void* list_get(List *list, s32 idx);
