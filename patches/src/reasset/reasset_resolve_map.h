#pragma once

#include "reasset_id.h"
#include "reasset_iterator.h"

typedef u32 ReAssetResolveMap;

void reasset_resolve_map_init(void);
ReAssetResolveMap reasset_resolve_map_create(const char *assetTypeName);
void reasset_resolve_map_finalize(ReAssetResolveMap map);
void reasset_resolve_map_resolve_id(ReAssetResolveMap map, ReAssetID id, ReAssetNamespace owner, s32 resolvedIdentifier);
s32 reasset_resolve_map_lookup(ReAssetResolveMap map, ReAssetID id);
ReAssetNamespace reasset_resolve_map_owner_of(ReAssetResolveMap map, s32 resolvedIdentifier);
ReAssetBool reasset_resolve_map_id_of(ReAssetResolveMap map, s32 resolvedIdentifier, ReAssetID *outID);
ReAssetBool reasset_resolve_map_id_data_of(ReAssetResolveMap map, s32 resolvedIdentifier, ReAssetIDData **outIDData);
ReAssetIterator reasset_resolve_map_create_iterator(ReAssetResolveMap map);
void reasset_resolve_map_link(ReAssetResolveMap map, ReAssetID id, ReAssetID externID);
