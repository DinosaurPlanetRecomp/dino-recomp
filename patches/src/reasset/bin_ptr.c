#include "bin_ptr.h"

void bin_ptr_set(BinPtr *binPtr, void *ptr, u32 offset, u32 size) {
    binPtr->ptr = (u8*)ptr + offset;
    binPtr->size = size;
}

void* bin_ptr_get(const BinPtr *binPtr, u32 *outSize) {
    if (outSize != NULL) {
        *outSize = binPtr->size;
    }
    
    return binPtr->ptr;
}
