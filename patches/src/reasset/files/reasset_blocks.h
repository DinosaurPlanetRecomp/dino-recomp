#pragma once

#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"

#include "PR/ultratypes.h"

void reasset_blocks_init(void);
void reasset_blocks_repack(void);
void reasset_blocks_patch(void);
void reasset_blocks_cleanup(void);
ReAssetResolveMap reasset_trkblk_get_resolve_map(void);
_Bool reasset_trkblk_is_base_id(s32 id);
ReAssetResolveMap reasset_blocks_get_resolve_map(ReAssetID trkblkID);
_Bool reasset_blocks_is_base_id(s32 trkblkID, s32 id);
