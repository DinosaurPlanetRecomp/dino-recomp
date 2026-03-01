#pragma once

#include "reasset/reasset_id.h"

#include "PR/ultratypes.h"

void reasset_dlls_init(void);
void reasset_dlls_repack(void);
_Bool reasset_dlls_is_base_id(s32 identifier);
s32 reasset_dlls_lookup(ReAssetID id);
