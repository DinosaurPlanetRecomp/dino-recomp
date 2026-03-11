#include "patches.h"

#include "PR/ultratypes.h"
#include "PR/gbi.h"
#include "sys/gfx/texture.h"
#include "sys/fs.h"
#include "sys/memory.h"
#include "sys/rarezip.h"
#include "macros.h"

extern s32 gTexAllocTag;

extern s32* gFile_TEX_TAB[2]; // TEX0/TEX1 tab
extern s32* gTextureCache;
extern u16* gFile_TEXTABLE;
extern s32 gNumCachedTextures;
extern s32 *gTexLoadBuffer; // scratch space used when loading texture bin header data

extern Gfx *tex_setup_display_lists(Texture *texture, Gfx *gdl);

// Note: this is based off of a nonmatch
RECOMP_PATCH Texture* tex_load(s32 id, u8 param2) {
    u32 binFileID; // sp74
    Texture* tex;
    s32 temp;
    s32 texID; // sp68
    s32 uncompressedSize;
    s32 compressedSize;
    s32 numFrames;
    s32 offset; // sp58
    s32 i;
    s32 tabEntry;
    s32 frame;
    s32 tab;
    Texture* firstTex; // sp44
    Texture* prevTex; // sp40
    u8* temp_s3;

    for (i = 0; i < gNumCachedTextures; i++) {
        if (id == gTextureCache[ASSETCACHE_ID(i)]) {
            tex = (Texture*)gTextureCache[ASSETCACHE_PTR(i)];
            tex->refCount += 1;
            return tex;
        }
    }

    texID = id;
    if (id < 0) {
        id = -id;
    } else {
        id = gFile_TEXTABLE[id];
    }
    tabEntry = id & 0xFFFF;
    if (tabEntry & 0x8000) {
        tab = 1; // TEX1
        binFileID = TEX1_BIN;
        tabEntry &= 0x7FFF;
    } else {
        tab = 0; // TEX0
        binFileID = TEX0_BIN;
    }
    
    offset = gFile_TEX_TAB[tab][tabEntry] & 0xFFFFFF;
    numFrames = (gFile_TEX_TAB[tab][tabEntry] >> 24) & 0xFF;
    compressedSize = (gFile_TEX_TAB[tab][tabEntry + 1] & 0xFFFFFF) - offset;
    if (numFrames > 1) {
        read_file_region(binFileID, gTexLoadBuffer, offset, (numFrames + 1) << 3);
    } else {
        gTexLoadBuffer[0] = 0;
        gTexLoadBuffer[1] = rarezip_uncompress_size_rom(binFileID, offset, 1);
        gTexLoadBuffer[2] = compressedSize;
    }
    
    firstTex = NULL;
    prevTex = NULL;
    for (frame = 0; frame < numFrames; frame++) {
        uncompressedSize = gTexLoadBuffer[(frame << 1) + 1];
        compressedSize = gTexLoadBuffer[(frame + 1) << 1] - gTexLoadBuffer[frame << 1];
        // @recomp: Support uncompressed textures
        _Bool notCompressed = uncompressedSize == -1;
        if (notCompressed) {
            uncompressedSize = compressedSize - 0x4;
        }
        tex = mmAlloc((uncompressedSize + 0xE4), gTexAllocTag, NULL);
        if (tex == NULL) {
            if ((frame + 1) == 1) {
                return NULL;
            }
            firstTex->animDuration = (s16) (numFrames << 8);
            frame = numFrames;
        } else {
            // @recomp: Support uncompressed textures (note: uncompressed data starts at +0x4 instead of +0x5)
            if (notCompressed) {
                read_file_region(binFileID, tex, gTexLoadBuffer[frame << 1] + offset + 0x4, uncompressedSize);
            } else {
                temp = (s32)((((u8*)tex + uncompressedSize) - compressedSize) + 0xE4);
                temp_s3 = (u8*)(temp - (temp % 16));
                read_file_region(binFileID, temp_s3, gTexLoadBuffer[frame << 1] + offset, compressedSize);
                rarezip_uncompress(temp_s3, (u8*)tex, uncompressedSize);
            }
            tex->next = NULL;
            if (prevTex != NULL) {
                prevTex->next = tex;
            }
            prevTex = tex;
            if (frame == 0) {
                firstTex = tex;
                tex->animDuration = (s16) (numFrames << 8);
            } else {
                tex->animDuration = 1;
            }
            tex->unk10 = (u32) (uncompressedSize + 0xE4) >> 2;
            mmRealloc(tex, 
                ((u8*)tex_setup_display_lists(tex, (Gfx*)mmAlign16((u32)tex + uncompressedSize)) - (u8*)tex) + 8, 
                NULL);
        }
    }

    for (i = 0; i < gNumCachedTextures; i++) {
        if (gTextureCache[ASSETCACHE_ID(i)] == -1) {
            break;
        }
    }
    
    if (i == gNumCachedTextures) {
        gNumCachedTextures += 1;
    }
    i <<= 1;
    gTextureCache[i] = texID;
    gTextureCache[i + 1] = (s32)firstTex;
    if (gNumCachedTextures > 700) {
        return NULL;
    }
    return firstTex;
}
