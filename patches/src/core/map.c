#include "patches.h"
#include "matrix_groups.h"
#include "rt64_extended_gbi.h"

#include "PR/ultratypes.h"
#include "PR/gbi.h"
#include "game/objects/object_id.h"
#include "sys/map.h"
#include "sys/fs.h"
#include "sys/memory.h"
#include "sys/rarezip.h"
#include "sys/asset_thread.h"
#include "sys/objprint.h"
#include "sys/objects.h"
#include "sys/vi.h"
#include "sys/menu.h"
#include "sys/segment_1D900.h"
#include "sys/dl_debug.h"
#include "sys/newshadows.h"
#include "sys/bitstream.h"
#include "sys/main.h"
#include "dll.h"
#include "gbi_extra.h"
#include "macros.h"

RECOMP_DECLARE_EVENT(recomp_on_block_loaded_rom(s32 id, Block *block));
RECOMP_DECLARE_EVENT(recomp_on_block_loaded(s32 id, Block *block));

extern s32* gFile_BLOCKS_TAB;
extern u8 *gMapReadBuffer;

extern s32 gMapCurrentStreamCoordsX;
extern s32 gMapCurrentStreamCoordsZ;

extern Gfx* gMainDL;
extern Mtx* gWorldRSPMatrices;
extern Vertex* D_800B51D4;
extern Triangle* D_800B51D8;
extern s8 gMapType;
extern Camera* D_800B51E4;
extern u32 UINT_80092a98;
extern u32 gRenderList[MAX_RENDER_LIST_LENGTH];
extern s16 gRenderListLength;
extern Block *gBlocksToDraw[MAX_BLOCKS];
extern s16 SHORT_800b51dc;
extern u32 UINT_800b51e0;
extern BlockTexture *gBlockTextures;
extern Block **gLoadedBlocks;
extern u8 gLoadedBlockCount;
extern s16 *gLoadedBlockIds;
extern s32 D_800B4A54;
extern s16 gBlocksToDrawIdx;
extern BitStream D_800B9780;
extern s8 D_80092B0C[176];
extern s8 *gBlockIndices[MAP_LAYER_COUNT];
extern s8 *D_800B9700[MAP_LAYER_COUNT];
extern s8 *D_800B9714;
extern f32 D_800B97B8;
extern f32 D_800B97BC;

extern void func_800436DC(Object* arg0, s32 arg1);
extern BlockTextureScroller* func_80049D68(s32 arg0);
extern void func_80044BEC(void);
extern void func_80048F58(void);
extern void track_c_func(void);

extern void func_80048B14(Block *block);
extern u32 hits_get_size(s32 id);
extern void block_setup_vertices(Block *block);
extern void block_setup_gdl_groups(Block *block);
extern s32 block_setup_textures(Block *block);
extern void block_setup_xz_bitmap(Block *block);
extern HitsLine* block_load_hits(Block *block, s32 blockID, u8 unused, HitsLine* hits_ptr);
extern void some_cell_func(BitStream* stream);
extern void func_80047404(s32, s32, s32*, s32*, s32*, s32*, s32, s32, s32);
extern s32 func_800451A0(s32 xPos, s32 zPos, Block* blocks);
extern void func_80043950(Block*, s16, s16, s16);
extern void block_compute_vertex_colors(Block*,s32,s32,s32);
extern void block_add_to_render_list(Block *block, f32 x, f32 z);
extern void func_80043FD8(s8* arg0);
extern void draw_render_list(Mtx *rspMtxs, s8 *visibilities);

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
    // @recomp: Invoke event
    recomp_on_block_loaded_rom(id, block);
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

    // @recomp: Invoke event
    recomp_on_block_loaded(id, block);

    if (queue != 0) {
        queue_block_emplace(1, (u32* ) block, (u8*)id, param_2, globalMapIdx);
    } else {
        block_emplace(block, id, param_2, globalMapIdx);
    }
}

RECOMP_PATCH void func_8004225C(Gfx** gdl, Mtx** mtxs, Vertex** vtxs, Triangle** pols, Vertex** vtxs2, Triangle** pols2) {
    Mtx* mtx;

    gMainDL = *gdl;
    gWorldRSPMatrices = *mtxs;
    D_800B51D4 = *vtxs;
    D_800B51D8 = *pols;
    UINT_80092a98 |= 0x21;
    if ((gMapType == MAPTYPE_MOBILE) || (gMapType == MAPTYPE_3)) {
        UINT_80092a98 &= ~1;
    }
    gSPTexture(gMainDL++, -1, -1, 3, 0, 1);
    mtx = get_some_model_view_mtx();
    gSPMatrix(gMainDL++, OS_K0_TO_PHYSICAL(mtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    camera_setup_viewport_and_matrices(&gMainDL, 0);
    // @recomp: Reset matrix tagging
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, G_EX_ID_AUTO, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    func_80044BEC();
    if (func_80010048() != 0) {
        if (!(UINT_80092a98 & 8)) {
            UINT_80092a98 |= 8;
        }
        camera_set_aspect(1.7777778f);
    } else if (UINT_80092a98 & 8) {
        UINT_80092a98 &= ~8;
        camera_set_aspect(1.3333334f);
    }
    if (UINT_80092a98 & 0x10000) {
        if (UINT_80092a98 & 8) {
            camera_set_aspect(1.7777778f);
        } else {
            camera_set_aspect(1.3333334f);
        }
        viewport_disable(get_camera_selector(), 0U);
        vi_some_video_setup(0);
        UINT_80092a98 &= ~0x10000;
    }
    if (UINT_80092a98 & 0x10) {
        setup_rsp_camera_matrices(&gMainDL, &gWorldRSPMatrices);
        gDLL_7_Newday->vtbl->func13(&gMainDL, &gWorldRSPMatrices);

        if (UINT_80092a98 & 0x40) {
            // @recomp: Tag newstars matrices
            gEXMatrixGroupSimpleNormal(gMainDL++, NEWSTARS_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            gDLL_10_Newstars->vtbl->func1(&gMainDL);
        }
        // @recomp: Tag newday matrices (stops the sun from flickering, auto order does not work)
        gEXMatrixGroupSimpleNormal(gMainDL++, NEWDAY_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
        gDLL_7_Newday->vtbl->func3(&gMainDL, &gWorldRSPMatrices, UINT_80092a98 & 0x40);
        // @recomp: Reset matrix tagging
        gEXMatrixGroupSimpleNormalAuto(gMainDL++, G_EX_ID_AUTO, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    } else {
        setup_rsp_camera_matrices(&gMainDL, &gWorldRSPMatrices);
    }
    gDLL_11_Newlfx->vtbl->func2();
    gDLL_57->vtbl->func3();
    gDLL_58->vtbl->func2();
    if (UINT_80092a98 & 0x20000) {
        if (gDLL_7_Newday->vtbl->func23(&gMainDL) == 0) {
            gDLL_8->vtbl->func3(&gMainDL);
        }
    } else {
        gDLL_8->vtbl->func3(&gMainDL);
    }
    D_800B51E4 = get_camera();
    func_80048F58();
    track_c_func();
    // @recomp: Tag newclouds matrices
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, NEWCLOUDS_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_9_Newclouds->vtbl->func4(&gMainDL);
    // @recomp: Reset matrix tagging
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, G_EX_ID_AUTO, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    camera_setup_fullscreen_viewport(&gMainDL);
    *gdl = gMainDL;
    *mtxs = gWorldRSPMatrices;
    *vtxs = D_800B51D4;
    *pols = D_800B51D8;
    UINT_80092a98 &= ~2;
    // @fake
    //if (1) { } if (1) { } if (1) { } if (1) { }
    // diProfEnd("Trackdraw") (default.dol)
}

RECOMP_PATCH void track_c_func(void) {
    s32 sp294;
    Block* var_s0;
    s32 temp_t2_2;
    s32 temp_t3;
    s32 temp_v1;
    s32 sp274[4];
    s32 sp264[4];
    s32 sp254[4];
    s32 sp244[4];
    s32 sp240;
    s32 var_s2;
    s32 var_s3;
    s32 temp_s1;
    s8* sp230;
    u8 sp130[BLOCKS_GRID_TOTAL_CELLS];
    u8 _pad[0x130-0x90];
    s8 *var_s8;
    s32 temp_v0;
    s32 i;
    s32 var_v0;
    s8 pad_sp7F;
    s8 pad_sp7E;
    s8 pad_sp7D;
    s8 sp7C;
    Mtx* sp78;

    dl_add_debug_info(gMainDL, 0, "track/track.c", 0x52B);
    some_cell_func(&D_800B9780);
    shadows_func_8004D9B8();
    shadows_func_8004DABC();
    gRenderListLength = 1;
    gBlocksToDrawIdx = 0;
    dl_add_debug_info(gMainDL, 0, "track/track.c", 0x53D);
    gDLL_9_Newclouds->vtbl->func6(&gMainDL, gUpdateRate, 0);
    // @recomp: Tag projgfx matrices
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, PROJGFX_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_15_Projgfx->vtbl->func5(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, 3);
    if (UINT_80092a98 & 0x10) {
        // @recomp: Tag minic matrices
        gEXMatrixGroupSimpleNormalAuto(gMainDL++, MINIC_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
        gDLL_12_Minic->vtbl->func3(&gMainDL, &gWorldRSPMatrices);
    }
    // @recomp: Reset matrix tagging
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, G_EX_ID_AUTO, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    dl_add_debug_info(gMainDL, 0, "track/track.c", 0x545);
    var_s8 = D_80092B0C;
    sp78 = gWorldRSPMatrices;
    sp240 = ARRAYCOUNT(gBlockIndices);
    while (--sp240 >= 0) {
        sp230 = gBlockIndices[sp240];
        D_800B9714 = D_800B9700[sp240];
        func_80047404(gMapCurrentStreamCoordsX + 7, gMapCurrentStreamCoordsZ + 7, sp274, sp264, sp254, sp244, sp240, 1, D_800B4A54);
        for (i = 0; i < (s32)ARRAYCOUNT(sp130); i++) { sp130[i] = 0; }
        
        for (var_s2 = sp274[2]; sp274[3] >= var_s2; var_s2++) {
            for (temp_s1 = sp274[0]; sp274[1] >= temp_s1; temp_s1++) {
                sp130[(temp_s1 + 7) + ((var_s2 + 7) << 4)] = 1;
            }
        }
        for (var_s2 = sp264[2]; sp264[3] >= var_s2; var_s2++) {
            for (temp_s1 = sp264[0]; sp264[1] >= temp_s1; temp_s1++) {
                sp130[(temp_s1 + 7) + ((var_s2 + 7) << 4)] = 1;
            }
        }
        for (var_s2 = sp254[2]; sp254[3] >= var_s2; var_s2++) {
            for (temp_s1 = sp254[0]; sp254[1] >= temp_s1; temp_s1++) {
                sp130[(temp_s1 + 7) + ((var_s2 + 7) << 4)] = 1;
            }
        }
        for (var_s2 = sp244[2]; sp244[3] >= var_s2; var_s2++) {
            for (temp_s1 = sp244[0]; sp244[1] >= temp_s1; temp_s1++) {
                sp130[(temp_s1 + 7) + ((var_s2 + 7) << 4)] = 1;
            }
        }
        // @fake
        //if (sp240){}

        for (sp294 = 0; sp294 < BLOCKS_GRID_SPAN; sp294++) {
            temp_s1 = var_s8[sp294];
            for (var_s3 = 0; var_s3 < BLOCKS_GRID_SPAN; var_s3++) {
                var_s2 = var_s8[var_s3];
                temp_v1 = GRID_INDEX(var_s2, temp_s1);
                temp_v0 = sp230[temp_v1];
                if (temp_v0 < 0) {
                    var_s0 = NULL;
                } else {
                    var_s0 = gLoadedBlocks[temp_v0];
                    var_s0->vtxFlags ^= 1;
                    if (sp130[temp_v1] == 0) {
                        continue;
                    }
                }
                if (temp_v0 < 0 || func_800451A0(temp_s1, var_s2, var_s0) == 0) {
                    continue;
                }
                D_800B97B8 = temp_s1 * BLOCKS_GRID_UNIT_F;
                D_800B97BC = var_s2 * BLOCKS_GRID_UNIT_F;
                func_80043950(var_s0, temp_s1, var_s2, sp240);
                if (UINT_80092a98 & 0x8000) {
                    if (var_s0->unk3E != 0) {
                        block_compute_vertex_colors(var_s0, temp_s1, var_s2, 0);
                    }
                    if ((var_s0->unk49 != 0) && (UINT_80092a98 & 0x100)) {
                        func_8001F4C0(var_s0, temp_s1, var_s2);
                    }
                }
                block_add_to_render_list(var_s0, D_800B97B8, D_800B97BC);
            }
        }
    }
    func_80043FD8(&sp7C);
    draw_render_list(sp78, &sp7C);
    dl_add_debug_info(gMainDL, 0, "track/track.c", 0x5B2);
    // @recomp: Tag projgfx matrices
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, PROJGFX_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_15_Projgfx->vtbl->func5(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, 2);
    gDLL_15_Projgfx->vtbl->func5(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, 1);
    // @recomp: Tag modgfx matrices
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, MODGFX_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_14_Modgfx->vtbl->func11(&sp7C);
    gDLL_14_Modgfx->vtbl->func6(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, 0, 0);
    // @recomp: Tag waterfx matrices
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, WATERFX_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_24_Waterfx->vtbl->func_C7C(&gMainDL, &gWorldRSPMatrices);
    // @recomp: Tag projgfx matrices
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, PROJGFX_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_15_Projgfx->vtbl->func5(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, 0);
    // @recomp: Reset matrix tagging
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, G_EX_ID_AUTO, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_2_Camera->vtbl->lock_icon_print(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, &D_800B51D8);
    gDLL_59_Minimap->vtbl->func1(&gMainDL, &gWorldRSPMatrices);
    shadows_func_8004D974(0);
    D_800B1847 = 0;
    dl_add_debug_info(gMainDL, 0, "track/track.c", 0x5C6);
}

static s32 recomp_get_block_id(Block *block) {
    for (s32 i = 0; i < gLoadedBlockCount; i++) {
        if (gLoadedBlocks[i] == block) {
            return gLoadedBlockIds[i];
        }
    }

    recomp_eprintf("Failed to find ID of block for matrix tagging.\n");
    return -1;
}

RECOMP_PATCH void draw_render_list(Mtx* rspMtxs, s8* visibilities) {
    BlockShape* shape;
    Vtx_t *tempVtx;
    s32 shapeIdx;
    BlockTextureScroller* temp_v0_7;
    s32 i;
    BlocksTextureIndexData* temp_v0_4;
    u32 spE4;
    s32 spE0;
    s32 spDC;
    s32 spD8;
    s32 spD4;
    s32 spD0;
    s32 spCC;
    EncodedTri* temp_a1;
    s32 spC4;
    EncodedTri* var_a0;
    EncodedTri* var_s2;
    Gfx* temp_s5;
    Texture* tex1;
    Texture* tex0;
    s32 temp_s0_2;
    s32 temp_v1;
    Mtx* spA4;
    s8 spA3;
    s8 temp2;
    s32 temp_t6;
    s32 var_s7;
    s32 var_t0;
    Block* block;
    Object** sp8C;
    Object *obj;

    spC4 = -1;
    sp8C = get_world_objects(NULL, NULL);
    gDLL_57->vtbl->func2(&spE0, &spDC, &spD8, &spD4, &spD0, &spCC);
    for (i = 1; i < gRenderListLength; i++) {
        temp_t6 = shapeIdx = (gRenderList[i] & 0x3F80) >> 7;
        if (gRenderList[i] & 0x40) {
            obj = sp8C[temp_t6];
            func_800436DC(obj, visibilities[temp_t6]);
            spA3 = 0;
        } else {
            // @fake
            //if (i) {}
            temp_v1 = gRenderList[i] & 0x3F;
            spE4 = 0;
            if (temp_v1 != spC4) {
                spA3 = -1;
                SHORT_800b51dc = -1;
                spC4 = temp_v1;
                UINT_800b51e0 = 0;
                spA4 = (temp_v1 * 2) + rspMtxs;
                block = gBlocksToDraw[temp_v1];
            }
            // @recomp: Tag block shape matrices
            //          TODO: animated water is jittery
            shape = &block->shapes[temp_t6];
            if (shape->flags & 0x20000000) {
                if (spA3 != 2) {
                    gSPMatrix(gMainDL++, OS_K0_TO_PHYSICAL(&spA4[1]), G_MTX_MODELVIEW | G_MTX_LOAD);
                    spA3 = 2;
                    gEXMatrixGroupSimpleNormalTcs(gMainDL++, (recomp_get_block_id(block) * 0x1000 * 2) + (temp_t6 * 2) + 1 + BLOCK_SHAPE_MTX_GROUP_ID_START, 
                        G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
                }
            } else if (spA3 != 1) {
                gSPMatrix(gMainDL++, OS_K0_TO_PHYSICAL(spA4), G_MTX_MODELVIEW | G_MTX_LOAD);
                spA3 = 1;
                gEXMatrixGroupSimpleNormalTcs(gMainDL++, (recomp_get_block_id(block) * 0x1000 * 2) + (temp_t6 * 2) + 0 + BLOCK_SHAPE_MTX_GROUP_ID_START, 
                    G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            }
            if (shape->materialIndex == 0xFF) {
                tex0 = NULL;
            } else {
                tex0 = block->materials[shape->materialIndex].texture;
            }
            if (shape->flags & 0x2000) {
                if (tex0->flags & 0xC000) {
                    dl_set_prim_color(&gMainDL, 0xFF, 0xFF, 0xFF, 0xA0);
                } else {
                    dl_set_prim_color(&gMainDL, 0xFF, 0xFF, 0xFF, 0x64);
                }
            } else {
                if (shape->envColourMode == 0xFF) {
                    dl_set_prim_color(&gMainDL, spE0, spDC, spD8, 0xFF);
                } else if (shape->envColourMode == 0xFE) {
                    dl_set_prim_color(&gMainDL, 0xFF, 0xFF, 0xFF, 0xFF);
                } else if (shape->flags & 0x46C00000) {
                    dl_set_prim_color(&gMainDL, 0xFF, 0xFF, 0xFF, 0xFF);
                } else {
                    func_8001F848(&gMainDL);
                }
            }
            var_s7 = shape->flags;
            if (var_s7 & 0x10000) {
                temp_v0_4 = func_8004A284(block, shape->animatorID);
                if (temp_v0_4 != NULL) {
                    var_t0 = gBlockTextures[temp_v0_4->textureIndex].unk4 << 8;
                    var_s7 |= gBlockTextures[temp_v0_4->textureIndex].flags;
                } else {
                    var_t0 = 0;
                }
                if ((shape->animatorID != SHORT_800b51dc) || (var_t0 != UINT_800b51e0)) {
                    SHORT_800b51dc = shape->animatorID;
                    UINT_800b51e0 = var_t0;
                    spE4 = 1;
                }
            } else {
                SHORT_800b51dc = -1;
                var_t0 = 0;
            }
            if (shape->blendMaterialIndex != 0xFF) {
                tex1 = block->materials[shape->blendMaterialIndex].texture;
            } else {
                tex1 = NULL;
            }
            tex_gdl_set_textures(&gMainDL, tex0, tex1, var_s7, var_t0, spE4, 0);
            if (shape->unk16 != 0xFF) {
                temp_v0_7 = func_80049D68(shape->unk16);
                gDPSetTileSize(gMainDL++, 0, temp_v0_7->uOffsetA, temp_v0_7->vOffsetA, (tex0->width - 1) << 2, (tex0->height - 1) << 2);
                if (tex1 != NULL) {
                    gDPSetTileSize(gMainDL++, 1, temp_v0_7->uOffsetB, temp_v0_7->vOffsetB, (tex1->width - 1) << 2, (tex1->height - 1) << 2);
                }
            } else if ((tex0 != NULL) && (tex0->flags & 0xC000)) {
                gDPSetTileSize(gMainDL++, 0, 0, 0, (tex0->width - 1) << 2, (tex0->height - 1) << 2);
                if (tex1 != NULL) {
                    gDPSetTileSize(gMainDL++, 1, 0, 0, (tex1->width - 1) << 2, (tex1->height - 1) << 2);
                }
            }
            shapeIdx = (shape - block->shapes) * 3;
            gMainDL->words.w0 = block->gdlGroups[shapeIdx].words.w0;
            gMainDL->words.w1 = block->gdlGroups[shapeIdx].words.w1;
            shapeIdx++;
            dl_apply_geometry_mode(&gMainDL);
            gMainDL->words.w0 = block->gdlGroups[shapeIdx].words.w0;
            gMainDL->words.w1 = block->gdlGroups[shapeIdx].words.w1;
            shapeIdx++;
            dl_apply_combine(&gMainDL);
            gMainDL->words.w0 = block->gdlGroups[shapeIdx].words.w0;
            gMainDL->words.w1 = block->gdlGroups[shapeIdx].words.w1;
            dl_apply_other_mode(&gMainDL);
            tempVtx = block->vertices2[(block->vtxFlags & 1) ^ 1];
            var_a0 = block->encodedTris;
            var_a0 += shape->triBase;
            temp_a1 = block->encodedTris;
            temp_a1 += shape[1].triBase;
            temp_s5 = gMainDL;

            gSPVertex2(gMainDL++, OS_K0_TO_PHYSICAL(&tempVtx[shape->vtxBase]), shape[1].vtxBase - shape->vtxBase, 0);

            var_s2 = NULL;
            while ((u32) var_a0 < (u32) temp_a1) {
                if (var_a0->d1 & 1) {
                    if (var_s2 == NULL) {
                        var_s2 = var_a0;
                    } else {
                        gSP2TrianglesBlock(gMainDL, var_s2->d0, var_a0->d0);
                        gMainDL++;
                        var_s2 = NULL;
                    }
                }
                var_a0++;
            }
            if ((var_s2 != NULL) && (var_s2->d1 & 1)) {
                gSP1TriangleBlock(gMainDL, var_s2->d0);
                gMainDL++;
            }
            gDLBuilder->needsPipeSync = 1;
            if ((var_s7 & 0x100408) == 0x100408) {
                temp_s0_2 = gMainDL - temp_s5;
                dl_set_geometry_mode(&gMainDL, 0x10000);
                if (var_s7 & 0x2004) {
                    gDPSetCombineLERP(gMainDL, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1);
                    dl_apply_combine(&gMainDL);
                    gDPSetOtherMode(
                        gMainDL, 
                        G_AD_PATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_1CYCLE | G_PM_NPRIMITIVE, 
                        G_AC_NONE | G_ZS_PIXEL | Z_CMP | IM_RD | CVG_DST_FULL | ZMODE_XLU | FORCE_BL | GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_MEM, G_BL_1MA) | GBL_c2(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_MEM, G_BL_1MA)
                    );
                    dl_apply_other_mode(&gMainDL);
                } else {
                    gDPSetCombineLERP(gMainDL, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1);
                    dl_apply_combine(&gMainDL);
                    
                    gDPSetOtherMode(
                        gMainDL, 
                        G_AD_PATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_PERSP | G_CYC_1CYCLE | G_PM_NPRIMITIVE, 
                        G_AC_NONE | G_ZS_PIXEL | AA_EN | Z_CMP | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_DEC | FORCE_BL | GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_MEM, G_BL_1MA) | GBL_c2(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_MEM, G_BL_1MA)
                    );
                    dl_apply_other_mode(&gMainDL);
                }
                bcopy(temp_s5, gMainDL, temp_s0_2 * sizeof(Gfx));
                gMainDL += temp_s0_2;
                gDLBuilder->needsPipeSync = 1;
            }
        }
    }
}

RECOMP_PATCH void func_800436DC(Object* obj, s32 arg1) {
    s8 sp37;
    u8 someBool;

    // @recomp: Get base matrix tagging group
    u32 objMtxGroup = recomp_obj_get_matrix_group(obj);

    someBool = TRUE;
    if ((obj->id == OBJ_IMSnowBike) || (obj->id == OBJ_CRSnowBike)) {
        someBool = TRUE;
        // TODO: snowbike dll
        if (((DLL_Unknown*)obj->dll)->vtbl->func[13].withOneArgS32((s32)obj) != 0) {
            someBool = FALSE;
        }
    }
    // @bug: sp37 is uninitialized if someBool is false
    if (someBool != FALSE) {
        sp37 = gDLL_13_Expgfx->vtbl->func10(obj);
    }
    // @recomp: Tag modgfx matrices
    gEXMatrixGroupDecomposedVertsOrderAuto(gMainDL++, objMtxGroup + OBJ_MODGFX_MTX_GROUP_ID_START, 
        G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_14_Modgfx->vtbl->func6(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, 1, obj);
    if (sp37 >= 2) {
        if ((obj->id != OBJ_IMSnowBike) && (obj->id != OBJ_CRSnowBike)) {
            gDLL_13_Expgfx->vtbl->func6(obj, &gMainDL, &gWorldRSPMatrices, &D_800B51D4, 1, 0, 0);
        }
    }
    objprint_func(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, &D_800B51D8, obj, arg1);
    if (sp37 != 0) {
        if ((obj->id != OBJ_IMSnowBike) && (obj->id != OBJ_CRSnowBike)) {
            gDLL_13_Expgfx->vtbl->func6(obj, &gMainDL, &gWorldRSPMatrices, &D_800B51D4, 0, 0, 0);
        }
    }
    if ((obj->linkedObject != NULL) && (arg1 != 0)) {
        sp37 = gDLL_13_Expgfx->vtbl->func10(obj->linkedObject);
        if (sp37 >= 2) {
            gDLL_13_Expgfx->vtbl->func6(obj->linkedObject, &gMainDL, &gWorldRSPMatrices, &D_800B51D4, 1, 0, 0);
        }
        if (sp37 != 0) {
            gDLL_13_Expgfx->vtbl->func6(obj->linkedObject, &gMainDL, &gWorldRSPMatrices, &D_800B51D4, 0, 0, 0);
        }
    }
}
