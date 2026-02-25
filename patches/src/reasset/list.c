#include "list.h"

#include "patches.h"

#include "reasset.h"

#include "PR/ultratypes.h"
#include "PR/os.h"

#define MAX_CAPACITY_INCREASE 256

// MARK: List

void list_init(List *list, s32 elementSize, s32 initialCapacity) {
    reasset_assert(list != NULL, "[reasset:list_init] List cannot be null!");
    reasset_assert(initialCapacity >= 0, "[reasset:list_init] Capacity cannot be negative!");

    if (initialCapacity > 0) {
        list->data = recomp_alloc(elementSize * initialCapacity);
        reasset_assert(list->data != NULL, "[reasset:list_init] List data alloc failed!");
        bzero(list->data, elementSize * initialCapacity);
    } else {
        list->data = NULL;
    }

    list->elementSize = elementSize;
    list->length = 0;
    list->capacity = initialCapacity;
}

void list_set_element_free_callback(List *list, ListElementFreeCallback callback) {
    reasset_assert(list != NULL, "[reasset:list_set_element_free_callback] List cannot be null!");

    list->elementFreeCallback = callback;
}

void list_free(List *list) {
    reasset_assert(list != NULL, "[reasset:list_free] List cannot be null!");

    if (list->data != NULL) {
        if (list->elementFreeCallback != NULL) {
            for (s32 i = 0; i < list->length; i++) {
                list->elementFreeCallback((u8*)list->data + (i * list->elementSize));
            }
        }

        recomp_free(list->data);
        list->data = NULL;
    }

    list->length = 0;
    list->capacity = 0;
}

void list_clear(List *list) {
    reasset_assert(list != NULL, "[reasset:list_clear] List cannot be null!");

    if (list->data != NULL && list->elementFreeCallback != NULL) {
        for (s32 i = 0; i < list->length; i++) {
            list->elementFreeCallback((u8*)list->data + (i * list->elementSize));
        }
    }

    list->length = 0;
}

void list_set_capacity(List *list, s32 capacity) {
    reasset_assert(list != NULL, "[reasset:list_set_capacity] List cannot be null!");
    reasset_assert(capacity >= 0, "[reasset:list_set_capacity] Capacity cannot be negative!");

    if (capacity != list->capacity) {
        u32 oldSize = list->elementSize * list->capacity;
        u32 newSize = list->elementSize * capacity;

        // Realloc buffer
        void *newData = recomp_alloc(newSize);
        reasset_assert(newData != NULL, "[reasset:list_set_capacity] List data alloc failed!");
        
        if (list->data != NULL) {
            // Free elements that are being removed, if any
            if (capacity < list->length && list->elementFreeCallback != NULL) {
                for (s32 i = capacity; i < list->length; i++) {
                    list->elementFreeCallback((u8*)list->data + (i * list->elementSize));
                }
            }
            // Copy old data over
            bcopy(list->data, newData, MIN(oldSize, newSize));
            // Zero out new memory
            if (newSize > oldSize) {
                bzero((u8*)newData + oldSize, newSize - oldSize);
            }
            // Free old buffer
            recomp_free(list->data);
        } else {
            // Zero out new buffer
            bzero(newData, newSize);
        }
        
        list->data = newData;
        list->capacity = capacity;
        // If capacity shrunk to less than the length, the length must also be shrunk
        list->length = MIN(list->length, list->capacity);
    }
}

void list_set_length(List *list, s32 length) {
    reasset_assert(list != NULL, "[reasset:list_set_length] List cannot be null!");
    reasset_assert(length >= 0, "[reasset:list_set_length] Length cannot be negative!");

    if (length < list->length && list->elementFreeCallback != NULL) {
        for (s32 i = length; i < list->length; i++) {
            list->elementFreeCallback((u8*)list->data + (i * list->elementSize));
        }
    }
    if (length > list->capacity) {
        list_set_capacity(list, length);
    }

    list->length = length;
}

s32 list_get_length(List *list) {
    reasset_assert(list != NULL, "[reasset:list_get_length] List cannot be null!");

    return list->length;
}

void* list_add(List *list) {
    reasset_assert(list != NULL, "[reasset:list_add] List cannot be null!");
    
    list->length += 1;

    if (list->length > list->capacity) {
        // Double capacity to avoid constant reallocs with additions
        if (list->capacity <= 0) {
            list_set_capacity(list, 1);
        } else {
            list_set_capacity(list, list->capacity + MIN(list->capacity, MAX_CAPACITY_INCREASE));
        }

        reasset_assert(list->capacity >= list->length, "[reasset:list_add] List capacity is less than length!?!?");
    }

    s32 idx = list->length - 1;

    return (u8*)list->data + (idx * list->elementSize);
}

s32 list_add_copy(List *list, const void *element) {
    s32 idx = list->length;
    void *item = list_add(list);

    bcopy(element, item, list->elementSize);

    return idx;
}

void* list_get(List *list, s32 idx) {
    reasset_assert(list != NULL, "[reasset:list_get] List cannot be null!");
    reasset_assert(idx >= 0, "[reasset:list_get] Index cannot be negative!");
    reasset_assert(idx < list->length, "[reasset:list_get] Index out of bounds! idx %d >= length %d", idx, list->length);

    return (u8*)list->data + (idx * list->elementSize);
}

// MARK: PtrList

void ptrlist_init(PtrList *list, s32 initialCapacity) {
    reasset_assert(list != NULL, "[reasset:ptrlist_init] List cannot be null!");
    reasset_assert(initialCapacity >= 0, "[reasset:ptrlist_init] Capacity cannot be negative!");

    if (initialCapacity > 0) {
        list->data = recomp_alloc(sizeof(void*) * initialCapacity);
        reasset_assert(list->data != NULL, "[reasset:ptrlist_init] List data alloc failed!");
        bzero(list->data, sizeof(void*) * initialCapacity);
    } else {
        list->data = NULL;
    }

    list->length = 0;
    list->capacity = initialCapacity;
}

void ptrlist_set_element_free_callback(PtrList *list, PtrListElementFreeCallback callback) {
    reasset_assert(list != NULL, "[reasset:ptrlist_set_element_free_callback] List cannot be null!");

    list->elementFreeCallback = callback;
}

void ptrlist_free(PtrList *list) {
    reasset_assert(list != NULL, "[reasset:ptrlist_free] List cannot be null!");

    if (list->data != NULL) {
        if (list->elementFreeCallback != NULL) {
            for (s32 i = 0; i < list->length; i++) {
                list->elementFreeCallback(list->data[i]);
            }
        }

        recomp_free(list->data);
        list->data = NULL;
    }

    list->length = 0;
    list->capacity = 0;
}

void ptrlist_clear(PtrList *list) {
    reasset_assert(list != NULL, "[reasset:ptrlist_clear] List cannot be null!");

    if (list->data != NULL && list->elementFreeCallback != NULL) {
        for (s32 i = 0; i < list->length; i++) {
            list->elementFreeCallback(list->data[i]);
        }
    }

    list->length = 0;
}

void ptrlist_set_capacity(PtrList *list, s32 capacity) {
    reasset_assert(list != NULL, "[reasset:ptrlist_set_capacity] List cannot be null!");
    reasset_assert(capacity >= 0, "[reasset:ptrlist_set_capacity] Capacity cannot be negative!");

    if (capacity != list->capacity) {
        u32 oldSize = sizeof(void*) * list->capacity;
        u32 newSize = sizeof(void*) * capacity;

        // Realloc buffer
        void *newData = recomp_alloc(newSize);
        reasset_assert(newData != NULL, "[reasset:ptrlist_set_capacity] List data alloc failed!");
        
        if (list->data != NULL) {
            // Free elements that are being removed, if any
            if (capacity < list->length && list->elementFreeCallback != NULL) {
                for (s32 i = capacity; i < list->length; i++) {
                    list->elementFreeCallback(list->data[i]);
                }
            }
            // Copy old data over
            bcopy(list->data, newData, MIN(oldSize, newSize));
            // Zero out new memory
            if (newSize > oldSize) {
                bzero((u8*)newData + oldSize, newSize - oldSize);
            }
            // Free old buffer
            recomp_free(list->data);
        } else {
            // Zero out new buffer
            bzero(newData, newSize);
        }
        
        list->data = newData;
        list->capacity = capacity;
        // If capacity shrunk to less than the length, the length must also be shrunk
        list->length = MIN(list->length, list->capacity);
    }
}

void ptrlist_set_length(PtrList *list, s32 length) {
    reasset_assert(list != NULL, "[reasset:ptrlist_set_length] List cannot be null!");
    reasset_assert(length >= 0, "[reasset:ptrlist_set_length] Length cannot be negative!");

    if (length < list->length && list->elementFreeCallback != NULL) {
        for (s32 i = length; i < list->length; i++) {
            list->elementFreeCallback(list->data[i]);
        }
    }
    if (length > list->capacity) {
        ptrlist_set_capacity(list, length);
    }

    list->length = length;
}

s32 ptrlist_get_length(PtrList *list) {
    reasset_assert(list != NULL, "[reasset:ptrlist_get_length] List cannot be null!");

    return list->length;
}

s32 ptrlist_add(PtrList *list, void *element) {
    reasset_assert(list != NULL, "[reasset:ptrlist_add] List cannot be null!");
    
    list->length += 1;

    if (list->length > list->capacity) {
        // Double capacity to avoid constant reallocs with additions
        if (list->capacity <= 0) {
            ptrlist_set_capacity(list, 1);
        } else {
            ptrlist_set_capacity(list, list->capacity + MIN(list->capacity, MAX_CAPACITY_INCREASE));
        }

        reasset_assert(list->capacity >= list->length, "[reasset:ptrlist_add] List capacity is less than length!?!?");
    }

    s32 idx = list->length - 1;
    list->data[idx] = element;

    return idx;
}

void* ptrlist_get(PtrList *list, s32 idx) {
    reasset_assert(list != NULL, "[reasset:ptrlist_get] List cannot be null!");
    reasset_assert(idx >= 0, "[reasset:ptrlist_get] Index cannot be negative!");
    reasset_assert(idx < list->length, "[reasset:ptrlist_get] Index out of bounds! idx %d >= length %d", idx, list->length);

    return list->data[idx];
}

// MARK: U32List

void u32list_init(U32List *list, s32 initialCapacity) {
    reasset_assert(list != NULL, "[reasset:u32list_init] List cannot be null!");
    reasset_assert(initialCapacity >= 0, "[reasset:u32list_init] Capacity cannot be negative!");

    if (initialCapacity > 0) {
        list->data = recomp_alloc(sizeof(u32) * initialCapacity);
        reasset_assert(list->data != NULL, "[reasset:u32list_init] List data alloc failed!");
        bzero(list->data, sizeof(u32) * initialCapacity);
    } else {
        list->data = NULL;
    }

    list->length = 0;
    list->capacity = initialCapacity;
}

void u32list_free(U32List *list) {
    reasset_assert(list != NULL, "[reasset:u32list_free] List cannot be null!");

    if (list->data != NULL) {
        recomp_free(list->data);
        list->data = NULL;
    }

    list->length = 0;
    list->capacity = 0;
}

void u32list_clear(U32List *list) {
    reasset_assert(list != NULL, "[reasset:u32list_clear] List cannot be null!");

    list->length = 0;
}

void u32list_set_capacity(U32List *list, s32 capacity) {
    reasset_assert(list != NULL, "[reasset:u32list_set_capacity] List cannot be null!");
    reasset_assert(capacity >= 0, "[reasset:u32list_set_capacity] Capacity cannot be negative!");

    if (capacity != list->capacity) {
        u32 oldSize = sizeof(u32) * list->capacity;
        u32 newSize = sizeof(u32) * capacity;

        // Realloc buffer
        void *newData = recomp_alloc(newSize);
        reasset_assert(newData != NULL, "[reasset:u32list_set_capacity] List data alloc failed!");
        
        if (list->data != NULL) {
            // Copy old data over
            bcopy(list->data, newData, MIN(oldSize, newSize));
            // Zero out new memory
            if (newSize > oldSize) {
                bzero((u8*)newData + oldSize, newSize - oldSize);
            }
            // Free old buffer
            recomp_free(list->data);
        } else {
            // Zero out new buffer
            bzero(newData, newSize);
        }
        
        list->data = newData;
        list->capacity = capacity;
        // If capacity shrunk to less than the length, the length must also be shrunk
        list->length = MIN(list->length, list->capacity);
    }
}

void u32list_set_length(U32List *list, s32 length) {
    reasset_assert(list != NULL, "[reasset:u32list_set_length] List cannot be null!");
    reasset_assert(length >= 0, "[reasset:u32list_set_length] Length cannot be negative!");

    if (length > list->capacity) {
        u32list_set_capacity(list, length);
    }

    list->length = length;
}

s32 u32list_get_length(U32List *list) {
    reasset_assert(list != NULL, "[reasset:u32list_get_length] List cannot be null!");

    return list->length;
}

s32 u32list_add(U32List *list, u32 element) {
    reasset_assert(list != NULL, "[reasset:u32list_add] List cannot be null!");
    
    list->length += 1;

    if (list->length > list->capacity) {
        // Double capacity to avoid constant reallocs with additions
        if (list->capacity <= 0) {
            u32list_set_capacity(list, 1);
        } else {
            u32list_set_capacity(list, list->capacity + MIN(list->capacity, MAX_CAPACITY_INCREASE));
        }

        reasset_assert(list->capacity >= list->length, "[reasset:u32list_add] List capacity is less than length!?!?");
    }

    s32 idx = list->length - 1;
    list->data[idx] = element;

    return idx;
}

u32 u32list_get(U32List *list, s32 idx) {
    reasset_assert(list != NULL, "[reasset:u32list_get] List cannot be null!");
    reasset_assert(idx >= 0, "[reasset:u32list_get] Index cannot be negative!");
    reasset_assert(idx < list->length, "[reasset:u32list_get] Index out of bounds! idx %d >= length %d", idx, list->length);

    return list->data[idx];
}

