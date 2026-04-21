#pragma once

#include "PR/ultratypes.h"

#include "reasset/reasset_resolve_map.h"

typedef enum TextureBank {
    TEX0,
    TEX1,
    NUM_TEXTURE_BANKS
} TextureBank;

void reasset_textures_init(void);
void reasset_textures_repack(void);
void reasset_textures_patch(void);
void reasset_textures_cleanup(void);
ReAssetResolveMap reasset_textures_get_resolve_map(TextureBank bank);
_Bool reasset_textures_is_base_id(TextureBank bank, s32 id);
