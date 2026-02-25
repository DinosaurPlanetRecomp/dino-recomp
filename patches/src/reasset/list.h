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

typedef void (*PtrListElementFreeCallback)(void *element);

typedef struct {
    void **data;
    s32 length;
    s32 capacity;
    PtrListElementFreeCallback elementFreeCallback;
} PtrList;

void list_init(List *list, s32 elementSize, s32 initialCapacity);
void list_set_element_free_callback(List *list, ListElementFreeCallback callback);
void list_free(List *list);
void list_clear(List *list);
void list_set_capacity(List *list, s32 capacity);
void list_set_length(List *list, s32 length);
s32 list_get_length(List *list);
/** Adds a new zeroed element. Returns a pointer to the new element. */
void* list_add(List *list);
/** Makes copy of element. Returns index of added element. */
s32 list_add_copy(List *list, const void *element);
void* list_get(List *list, s32 idx);

void ptrlist_init(PtrList *list, s32 initialCapacity);
void ptrlist_set_element_free_callback(PtrList *list, PtrListElementFreeCallback callback);
void ptrlist_free(PtrList *list);
void ptrlist_clear(PtrList *list);
void ptrlist_set_capacity(PtrList *list, s32 capacity);
void ptrlist_set_length(PtrList *list, s32 length);
s32 ptrlist_get_length(PtrList *list);
s32 ptrlist_add(PtrList *list, void *element);
void* ptrlist_get(PtrList *list, s32 idx);
