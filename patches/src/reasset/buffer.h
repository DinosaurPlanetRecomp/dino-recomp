#pragma once

#include "PR/ultratypes.h"

typedef struct {
    void *ptr;
    u32 size;
    u32 capacity;
    struct {
        u8 isSet;
        u8 fileID;
        u32 offset;
        u32 size;
    } base;
} Buffer;

void buffer_init(Buffer *buffer, u32 initialCapacity);
void buffer_free(Buffer *buffer);
void buffer_set(Buffer *buffer, const void *data, u32 size);
void* buffer_get(Buffer *buffer, u32 *outSize);
u32 buffer_get_size(const Buffer *buffer);
_Bool buffer_is_set(const Buffer *buffer);
void buffer_zero(Buffer *buffer, u32 size);
void buffer_resize(Buffer *buffer, u32 size);
void buffer_copy_to(const Buffer *buffer, void *dst, u32 offset);
void buffer_load_from_file(Buffer *buffer, s32 fileID, u32 offset, u32 size);
void buffer_set_base(Buffer *buffer, s32 fileID, u32 offset, u32 size);
