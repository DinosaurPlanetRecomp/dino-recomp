#pragma once

#include "reasset/reasset_resolve_map.h"

#include "PR/ultratypes.h"

void reasset_sequences_init(void);
void reasset_sequences_repack(void);
void reasset_sequences_patch(void);
void reasset_sequences_cleanup(void);
_Bool reasset_object_sequences_is_base_id(s32 id);
ReAssetResolveMap reasset_object_sequences_get_resolve_map(void);
