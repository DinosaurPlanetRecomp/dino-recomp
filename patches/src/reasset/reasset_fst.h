#pragma once

#include "PR/ultratypes.h"

void reasset_fst_init(void);
void reasset_fst_rebuild(void);
void reasset_fst_set_internal(s32 fileID, void *data, u32 size, _Bool ownedByReAsset);
u32 reasset_fst_get_file_size(s32 fileID);
void reasset_fst_read_from_file(s32 fileID, void *dst, u32 offset, u32 size);
s32 reasset_fst_audio_dma(void *dst, u32 romAddr, u32 size);
