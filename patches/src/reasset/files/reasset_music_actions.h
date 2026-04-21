#pragma once

#include "reasset/reasset_resolve_map.h"

void reasset_music_actions_init(void);
void reasset_music_actions_repack(void);
void reasset_music_actions_cleanup(void);
ReAssetResolveMap reasset_music_actions_get_resolve_map(void);
_Bool reasset_music_actions_is_base_id(s32 id);
