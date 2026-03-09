#pragma once

#include "reasset/reasset_resolve_map.h"

#include "PR/ultratypes.h"

void reasset_anims_init(void);
void reasset_anims_repack(void);
void reasset_anims_cleanup(void);
ReAssetResolveMap reasset_anims_get_resolve_map(void);
_Bool reasset_anims_is_base_id(s32 id);
