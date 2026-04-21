#pragma once

#include "reasset/reasset_resolve_map.h"

#include "PR/ultratypes.h"

void reasset_objects_init(void);
void reasset_objects_repack(void);
void reasset_objects_patch(void);
void reasset_objects_cleanup(void);
_Bool reasset_object_indices_is_base_id(s32 id);
ReAssetResolveMap reasset_object_indices_get_resolve_map(void);
