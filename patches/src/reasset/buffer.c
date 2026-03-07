#include "buffer.h"

#include "patches.h"
#include "reasset.h"
#include "reasset/reasset_fst.h"
#include "reasset/bin_ptr.h"

#include "PR/ultratypes.h"
#include "PR/os.h"

static struct {
    void *data;
    u32 capacity;
} sTempBuffer = {0};

void buffer_init(Buffer *buffer, u32 initialCapacity) {
    reasset_assert(buffer != NULL, "[reasset:buffer_init] Buffer cannot be null!");
    reasset_assert(buffer->ptr == NULL, "[reasset:buffer_init] Buffer is already initialized!");

    if (initialCapacity > 0) {
        buffer->ptr = recomp_alloc(initialCapacity);
    } else {
        buffer->ptr = NULL;
    }

    buffer->size = 0;
    buffer->capacity = initialCapacity;
    buffer->base.isSet = FALSE;
}

void buffer_free(Buffer *buffer) {
    if (buffer->ptr != NULL) {
        recomp_free(buffer->ptr);
        buffer->ptr = NULL;
    }

    buffer->size = 0;
    buffer->capacity = 0;
}

void buffer_set(Buffer *buffer, const void *data, u32 size) {
    reasset_assert(buffer != NULL, "[reasset:buffer_set] Buffer cannot be null!");

    if (size == 0) {
        buffer->size = 0;
        return;
    }

    if (buffer->ptr == NULL) {
        // Buffer not allocated yet
        buffer->ptr = recomp_alloc(size);
        buffer->capacity = size;
    } else if (size > buffer->capacity) {
        // Buffer too small, reallocate
        recomp_free(buffer->ptr);
        buffer->ptr = recomp_alloc(size);
        buffer->capacity = size;
    }

    bcopy(data, buffer->ptr, size);
    buffer->size = size;
}

void* buffer_get(Buffer *buffer, u32 *outSize) {
    reasset_assert(buffer != NULL, "[reasset:buffer_set] Buffer cannot be null!");

    if ((buffer->ptr == NULL || buffer->size == 0) && buffer->base.isSet) {
        // Lazy load from base file
        buffer_load_from_file(buffer, buffer->base.fileID, buffer->base.offset, buffer->base.size);
    }

    if (outSize != NULL) {
        *outSize = buffer->size;
    }

    return buffer->ptr;
}

u32 buffer_get_size(const Buffer *buffer) {
    reasset_assert(buffer != NULL, "[reasset:buffer_get_size] Buffer cannot be null!");

    if ((buffer->ptr == NULL || buffer->size == 0) && buffer->base.isSet) {
        return buffer->base.size;
    }

    return buffer->size;
}

_Bool buffer_is_set(const Buffer *buffer) {
    reasset_assert(buffer != NULL, "[reasset:buffer_is_set] Buffer cannot be null!");

    return buffer->ptr != NULL && buffer->size > 0;
}

void buffer_zero(Buffer *buffer, u32 size) {
    reasset_assert(buffer != NULL, "[reasset:buffer_zero] Buffer cannot be null!");

    if (size == 0) {
        buffer->size = 0;
        return;
    }

    if (buffer->ptr == NULL) {
        // Buffer not allocated yet
        buffer->ptr = recomp_alloc(size);
        buffer->capacity = size;
    } else if (size > buffer->capacity) {
        // Buffer too small, reallocate
        recomp_free(buffer->ptr);
        buffer->ptr = recomp_alloc(size);
        buffer->capacity = size;
    }

    buffer->size = size;
    bzero(buffer->ptr, size);
}

void buffer_resize(Buffer *buffer, u32 size) {
    reasset_assert(buffer != NULL, "[reasset:buffer_resize] Buffer cannot be null!");

    if (size == 0) {
        buffer->size = 0;
        return;
    }

    if (buffer->ptr == NULL) {
        // Buffer not allocated yet
        buffer->ptr = recomp_alloc(size);
        buffer->capacity = size;
        bzero(buffer->ptr, size);
    } else if (size > buffer->capacity) {
        // Buffer too small, reallocate
        void *newPtr = recomp_alloc(size);
        bcopy(buffer->ptr, newPtr, buffer->size);
        bzero((u8*)newPtr + size, size - buffer->size);
        recomp_free(buffer->ptr);
        buffer->ptr = newPtr;
        buffer->capacity = size;
    }

    buffer->size = size;
}

void buffer_copy_to(const Buffer *buffer, void *dst, u32 offset) {
    reasset_assert(buffer != NULL, "[reasset:buffer_copy_to] Buffer cannot be null!");

    u8 *_dst = (u8*)dst + offset;

    if (buffer->ptr == NULL || buffer->size == 0) {
        // Copy from base if set
        if (buffer->base.isSet && buffer->base.size > 0) {
            if ((u32)_dst & 0x7) {
                // Unaligned dest, load into temp buffer first
                if (sTempBuffer.data == NULL || buffer->base.size > sTempBuffer.capacity) {
                    // Temp buffer too small
                    if (sTempBuffer.data == NULL) {
                        recomp_free(sTempBuffer.data);
                    }
                    sTempBuffer.data = recomp_alloc(buffer->base.size);
                    sTempBuffer.capacity = buffer->base.size;
                }

                reasset_fst_read_from_file(buffer->base.fileID, sTempBuffer.data, buffer->base.offset, buffer->base.size);
                bcopy(sTempBuffer.data, _dst, buffer->base.size);
            } else {
                reasset_fst_read_from_file(buffer->base.fileID, _dst, buffer->base.offset, buffer->base.size);
            }
        }
    } else {
        bcopy(buffer->ptr, _dst, buffer->size);
    }
}

void buffer_copy_to_bin_ptr(const Buffer *buffer, const BinPtr *binPtr) {
    reasset_assert(buffer != NULL, "[reasset:buffer_copy_to_bin_ptr] Buffer cannot be null!");
    reasset_assert(binPtr != NULL, "[reasset:buffer_copy_to_bin_ptr] Bin ptr cannot be null!");

    reasset_assert(buffer->size <= binPtr->size, 
        "[reasset:buffer_copy_to_bin_ptr] Buffer is too large for the bin ptr destination! %d > %d",
        buffer->size, binPtr->size);

    buffer_copy_to(buffer, binPtr->ptr, 0);
}

void buffer_load_from_file(Buffer *buffer, s32 fileID, u32 offset, u32 size) {
    reasset_assert(buffer != NULL, "[reasset:buffer_load_from_file] Buffer cannot be null!");

    if (size == 0) {
        buffer->size = 0;
        return;
    }

    if (buffer->ptr == NULL) {
        // Buffer not allocated yet
        buffer->ptr = recomp_alloc(size);
        buffer->capacity = size;
    } else if (size > buffer->capacity) {
        // Buffer too small, reallocate
        recomp_free(buffer->ptr);
        buffer->ptr = recomp_alloc(size);
        buffer->capacity = size;
    }

    reasset_fst_read_from_file(fileID, buffer->ptr, offset, size);
    buffer->size = size;
}

void buffer_set_base(Buffer *buffer, s32 fileID, u32 offset, u32 size) {
    reasset_assert(buffer != NULL, "[reasset:buffer_set_base] Buffer cannot be null!");
    // PI DMA ROM addresses cannot be odd numbers. The ROM address of the file's start will
    // always be aligned, so we just need to verify the offset.
    reasset_assert((offset & 0x1) == 0, 
        "[reasset:buffer_set_base] Unaligned base offsets are not supported. fileID: %d  offset: %d  size: %d", 
        fileID, offset ,size);

    buffer->base.fileID = fileID;
    buffer->base.offset = offset;
    buffer->base.size = size;
    buffer->base.isSet = TRUE;
}
