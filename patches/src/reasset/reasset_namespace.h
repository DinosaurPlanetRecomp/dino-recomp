#pragma once

#include "PR/ultratypes.h"

#define REASSET_BASE_NAMESPACE 0
#define REASSET_INVALID_NAMESPACE ((ReAssetNamespace)-1)

typedef u32 ReAssetNamespace;

typedef struct {
    const char *name;
} ReAssetNamespaceData;

void reasset_namespace_init(void);
_Bool reasset_namespace_lookup_name(ReAssetNamespace namespace, const char **outName);
ReAssetNamespace reasset_namespace(const char *name);
