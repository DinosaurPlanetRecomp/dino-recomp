#pragma once

#include "reasset/reasset_resolve_map.h"

#include "PR/ultratypes.h"

void reasset_models_init(void);
void reasset_models_repack(void);
void reasset_models_patch(void);
void reasset_models_cleanup(void);
_Bool reasset_models_is_base_id(s32 id);
ReAssetResolveMap reasset_models_get_resolve_map(void);
