#pragma once

#include "reasset/reasset_namespace.h"

#include "PR/ultratypes.h"

/**
 * A namespace:identifier pair.
 * If namespace is zero, this represents a base asset.
 */
typedef u32 ReAssetID;

typedef struct {
    ReAssetNamespace namespace;
    s32 identifier;
} ReAssetIDData;

void reasset_id_init(void);
ReAssetIDData *reasset_id_lookup_data(ReAssetID id);
ReAssetIDData *reasset_id_lookup_data_or_null(ReAssetID id);
_Bool reasset_id_lookup_name(ReAssetID id, const char **outNamespaceName, s32 *outIdentifier);
ReAssetID reasset_id(ReAssetNamespace namespace, s32 identifier);
ReAssetID reasset_base_id(s32 identifier);
