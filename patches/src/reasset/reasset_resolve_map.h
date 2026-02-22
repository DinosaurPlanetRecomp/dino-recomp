#pragma once

#include "reasset_id.h"

typedef u32 ReAssetResolveMap;

void reasset_resolve_map_init(void);
ReAssetResolveMap reasset_resolve_map_create(const char *assetTypeName);
void reasset_resolve_map_finalize(ReAssetResolveMap map);
void reasset_resolve_map_resolve_id(ReAssetResolveMap map, ReAssetID id, s32 resolvedIdentifier, void *resolvedPtr);
void reasset_resolve_map_link(ReAssetResolveMap map, ReAssetID id, ReAssetID externID);
