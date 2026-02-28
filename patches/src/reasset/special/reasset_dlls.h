#pragma once

#include "reasset/reasset_id.h"

#include "PR/ultratypes.h"

void reasset_dlls_init(void);
void reasset_dlls_repack(void);
s32 reasset_dlls_lookup(ReAssetID id);
