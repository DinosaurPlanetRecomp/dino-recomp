#pragma once

#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"

#include "PR/ultratypes.h"

void reasset_maps_init(void);
void reasset_maps_repack(void);
void reasset_maps_patch(void);
void reasset_maps_cleanup(void);
ReAssetResolveMap reasset_maps_get_resolve_map(void);
_Bool reasset_map_objects_is_base_uid(ReAssetID mapID, s32 uid);
_Bool reasset_map_objects_is_base_global_uid(s32 uid);
ReAssetResolveMap reasset_map_objects_get_resolve_map(ReAssetID mapID);
ReAssetResolveMap reasset_map_objects_get_global_resolve_map(void);
