#pragma once

#include "PR/ultratypes.h"

typedef struct {
    void *ptr;
    u32 size;
} BinPtr;

void bin_ptr_set(BinPtr *binPtr, void *ptr, u32 offset, u32 size);
void* bin_ptr_get(const BinPtr *binPtr, u32 *outSize);
