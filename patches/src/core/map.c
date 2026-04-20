#include "patches.h"

#include "PR/ultratypes.h"
#include "PR/gbi.h"
#include "sys/map.h"
#include "sys/fs.h"
#include "sys/memory.h"
#include "sys/rarezip.h"
#include "sys/asset_thread.h"

extern s32* gFile_BLOCKS_TAB;
extern u8 *gMapReadBuffer;

extern void func_80048B14(Block *block);
extern u32 hits_get_size(s32 id);
extern void block_setup_vertices(Block *block);
extern void block_setup_gdl_groups(Block *block);
extern s32 block_setup_textures(Block *block);
extern void block_setup_xz_bitmap(Block *block);
extern HitsLine* block_load_hits(Block *block, s32 blockID, u8 unused, HitsLine* hits_ptr);

RECOMP_PATCH void block_load(s32 id, s32 param_2, s32 globalMapIdx, u8 queue) {
    s32 texIdx;
    s32 binOffset;
    s32 binSize;
    s32 size;
    s32 vtxIdx;
    Vtx_t* verts;
    Block* block;
    BlockShape* shape;
    BlockVertex* fileVerts;
    BlockVertex* fileVertsEnd;
    u32 addr;
    s32 pad[3];
    s32 tempLoadAddr;

    binOffset = gFile_BLOCKS_TAB[id];
    binSize = gFile_BLOCKS_TAB[id + 1] - binOffset;
    read_file_region(BLOCKS_BIN, gMapReadBuffer, binOffset, 0x10);
    size = ((s32*)gMapReadBuffer)[0];
    size += hits_get_size(id) + 8;
    block = mmAlloc(size, ALLOC_TAG_TRACK_COL, NULL);
    if (block == NULL) {
        return;
    }

    //if (0) { if ((s32)&size) {} } // @fake

    // @recomp: Support uncompressed blocks
    _Bool isUncompressed = rarezip_uncompress_size((u8*)gMapReadBuffer + 4) == -1;
    if (isUncompressed) {
        // Note: Uncompressed data starts at +0x8 instead of +0x9
        read_file_region(BLOCKS_BIN, block, binOffset + 8, binSize - 8);
    } else {
        tempLoadAddr = (((u32)block + size) - binSize) - 0x10;
        tempLoadAddr -= tempLoadAddr % 16;
        read_file_region(BLOCKS_BIN, (void*)tempLoadAddr, binOffset, binSize);
        rarezip_uncompress(((u8*)tempLoadAddr) + 4, (u8*)block, size);
    }
    block->vertices = (BlockVertex*)((u32)block->vertices + (u32)block);
    block->encodedTris = (EncodedTri*)((u32)block->encodedTris + (u32)block);
    block->shapes = (BlockShape*)((u32)block->shapes + (u32)block);
    block->ptr_faceEdgeVectors = (s16*)((u32)block->ptr_faceEdgeVectors + (u32)block);
    block->materials = (BlocksMaterial*)((u32)block->materials + (u32)block);
    tex_set_alloc_tag(ALLOC_TAG_TRACKTEX_COL);
    for (texIdx = 0; texIdx < block->materialCount; texIdx++) {
        block->materials[texIdx].texture = tex_load(-((u32)block->materials[texIdx].texture | 0x8000), queue);
    }
    tex_set_alloc_tag(ALLOC_TAG_TEX_COL);
    block_setup_vertices(block);
    addr = (u32)block;
    addr += block->modelSize;
    block->gdlGroups = (Gfx*)addr;
    block_setup_gdl_groups(block);
    addr += (3 * block->shapeCount * sizeof(Gfx));
    func_80048B14(block);
    if (block->vtxFlags & 8) {
        addr = mmAlign8(addr);
        fileVerts = block->vertices;
        block->vertices2[0] = (Vtx_t*)addr;
        addr += (sizeof(Vtx_t) * block->vtxCount);
        block->vertices2[1] = (Vtx_t*)addr;
        addr += (sizeof(Vtx_t) * block->vtxCount);
        
        verts = block->vertices2[0];
        vtxIdx = 0;
        shape = block->shapes;
        fileVertsEnd = fileVerts;
        //if (block->vertices) {} // @fake
        //if (block->vtxCount && block->vtxCount){} // @fake
        fileVertsEnd += block->vtxCount;
        while (fileVerts < fileVertsEnd) {
            if (shape->flags & 0x20000000) {
                verts->ob[0] = (f32) fileVerts->ob[0];
                verts->ob[1] = (fileVerts->ob[1] - block->minY) * 20.0f;
                verts->ob[2] = (f32) fileVerts->ob[2];
            } else {
                verts->ob[0] = fileVerts->ob[0];
                verts->ob[1] = fileVerts->ob[1];
                verts->ob[2] = fileVerts->ob[2];
            }
            verts->cn[0] = fileVerts->cn[0];
            verts->cn[1] = fileVerts->cn[1];
            verts->cn[2] = fileVerts->cn[2];
            verts->cn[3] = fileVerts->cn[3];
            verts->tc[0] = fileVerts->tc[0];
            verts->tc[1] = fileVerts->tc[1];
            verts->flag = fileVerts->flag;
            vtxIdx += 1;
            fileVerts += 1;
            verts += 1;
            if (vtxIdx >= (shape + 1)->vtxBase) {
                shape += 1;
            }
        }
        bcopy(block->vertices2[0], block->vertices2[1], sizeof(Vtx_t) * block->vtxCount);
    } else {
        block->vertices2[0] = (Vtx_t*)block->vertices;
        block->vertices2[1] = (Vtx_t*)block->vertices;
    }
    addr = mmAlign4(addr);
    block->unk28 = (BlocksTextureIndexData*)addr;
    addr += block_setup_textures(block);
    addr = mmAlign2(addr);
    block->xzBitmap = (s16*)addr;
    addr += block->unk34 * 2;
    block_setup_xz_bitmap(block);
    block_load_hits(block, id, queue, (HitsLine*)mmAlign8(addr));

    // @recomp: Restore printf
    // TODO: this trips for most blocks... is this calculation right?
    // s32 allocSize = ((s32*)gMapReadBuffer)[0];
    // s32 actualSize = addr - (u32)block;
    // if (actualSize != allocSize) {
    //     recomp_eprintf("Blocksize error(1): %d should be %d\n", actualSize, allocSize);
    // }

    if (queue != 0) {
        queue_block_emplace(1, (u32* ) block, (u8*)id, param_2, globalMapIdx);
    } else {
        block_emplace(block, id, param_2, globalMapIdx);
    }
}
