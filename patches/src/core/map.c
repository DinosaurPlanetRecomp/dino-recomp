#include "patches.h"
#include "patches/map.h"
#include "matrix_groups.h"
#include "recomp_funcs.h"
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

// TODO: use from map.h
#define MAX_VISIBLE_OBJECTS 180

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
extern s16 gRenderListLength;
extern Block *gBlocksToDraw[MAX_BLOCKS];
extern s16 SHORT_800b51dc;
extern s32 UINT_800b51e0;
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
extern BlockColorTableEntry *gBlockColorTable; // 255 items
extern u8 *gBlockRefCounts;
extern GlobalMapCell *gDecodedGlobalMap[MAP_LAYER_COUNT]; //16*16 grid of GlobalMapCell structs, one for each layer!
extern u16 *gFile_TRKBLK;
extern u32 *gFile_HITS_TAB;
extern s32* gFile_MAPS_TAB; // unknown pointer type
extern MapHeader* gLoadedMapsDataTable[120];
extern s16 gNumTRKBLKEntries;
extern s32 gNumTotalBlocks;
extern SavedObject* D_800B96B0;
extern s16 D_800B4A5C;
extern BlockTextureScroller *D_800B97A8; //gMapTextureScrollers?
extern Plane gFrustumPlanes[MAP_LAYER_COUNT];
extern BitStream D_800B9780;
extern u8 D_800B9794;
extern u8 D_800B979C;

extern void func_800436DC(Object* obj, s32 visibility);
extern BlockTextureScroller* func_80049D68(s32 arg0);
extern void func_80044BEC(void);
extern void func_80048F58(void);
extern void track_c_func(void);

extern void block_color_table_add_block(Block *block);
extern u32 hits_get_size(s32 id);
extern void block_setup_vertices(Block *block);
extern void block_setup_gdl_groups(Block *block);
extern s32 block_setup_textures(Block *block);
extern void block_setup_xz_bitmap(Block *block);
extern HitsLine* block_load_hits(Block *block, s32 blockID, u8 unused, HitsLine* hits_ptr);
extern void some_cell_func(BitStream* stream);
extern void func_80047404(s32, s32, s32*, s32*, s32*, s32*, s32, s32, s32);
extern s32 func_800451A0(s32 xPos, s32 zPos, Block* blocks);
extern void block_calc_shape_visibility(Block*, s16, s16, s16);
extern void block_compute_vertex_colors(Block*,s32,s32,s32);
extern void block_add_to_render_list(Block *block, f32 x, f32 z);
extern void func_80043FD8(s8* arg0);
extern void draw_render_list(Mtx *rspMtxs, s8 *visibilities);
extern void func_800441F4(u32* arg0, s32 arg1);
extern s32 func_80045600(s32 arg0, BitStream *stream, s16 arg2, s16 arg3, s16 arg4);

typedef struct {
    s32 x;
    s32 z;
    s32 layer;
    s32 blockIndex;
} RecompBlockGridInfo;

static RecompBlockGridInfo recomp_blocksToDrawGridCells[MAX_BLOCKS];
static RecompBlockGridInfo recomp_addBlockToRenderListGridInfo;

typedef struct {
    u8 skipInterpolation;
} RecompBlockInterpState;

#define RECOMP_RENDER_LIST_LENGTH (MAX_RENDER_LIST_LENGTH * 8)

static u32 recomp_RenderList[RECOMP_RENDER_LIST_LENGTH];
static RecompBlockInterpState recomp_blockInterpStates[MAX_BLOCKS];

s32 recomp_cpuBlockShapeCulling;

RECOMP_PATCH void init_maps(void) {
    s32 i;

    UINT_80092a98 = 0;
    gBlockColorTable = mmAlloc(sizeof(BlockColorTableEntry) * 255, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:cblocks"));
    gLoadedBlocks = mmAlloc(sizeof(Block*) * MAX_BLOCKS, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:blknos"));
    gLoadedBlockIds = mmAlloc(sizeof(s16) * MAX_BLOCKS, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:blkusage"));
    gBlockRefCounts = mmAlloc(sizeof(u8) * MAX_BLOCKS, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:mapinfo"));
    gMapReadBuffer = mmAlloc(sizeof(u8) * 700, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:tempbuf"));
    *gBlockIndices = mmAlloc(BLOCKS_GRID_TOTAL_CELLS * MAP_LAYER_COUNT, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:blkmaps"));
    *gDecodedGlobalMap = mmAlloc(sizeof(GlobalMapCell) * 1280, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:entrymaps"));
    *D_800B9700 = mmAlloc(BLOCKS_GRID_TOTAL_CELLS * MAP_LAYER_COUNT, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:vismap"));
    for (i = 1; i < MAP_LAYER_COUNT; i++) {
        gBlockIndices[i] = gBlockIndices[i - 1] + BLOCKS_GRID_TOTAL_CELLS;
        gDecodedGlobalMap[i] = gDecodedGlobalMap[i - 1] + BLOCKS_GRID_TOTAL_CELLS;
        D_800B9700[i] = D_800B9700[i - 1] + BLOCKS_GRID_TOTAL_CELLS;
    }
    queue_alloc_load_file((void **) &gFile_MAPS_TAB, MAPS_TAB);
    queue_alloc_load_file((void** ) &gFile_HITS_TAB, HITS_TAB);
    for (i = 0; i < 120; i++) { gLoadedMapsDataTable[i] = NULL; }
    queue_alloc_load_file((void** ) &gFile_TRKBLK, TRKBLK_BIN);
    gNumTRKBLKEntries = 0;
    while (gFile_TRKBLK[gNumTRKBLKEntries] != 0xFFFF) {
        gNumTRKBLKEntries++;
    }
    gNumTRKBLKEntries--;
    queue_alloc_load_file((void **) &gFile_BLOCKS_TAB, BLOCKS_TAB);
    gNumTotalBlocks = 0;
    while (gFile_BLOCKS_TAB[gNumTotalBlocks] != -1) {
        gNumTotalBlocks++;
    }
    gNumTotalBlocks--;
    D_800B96B0 = mmAlloc(sizeof(SavedObject) * 100, ALLOC_TAG_TRACK_COL, ALLOC_NAME("objdef_store"));
    D_800B4A5C = -1;
    D_800B4A5E = -2;
    gBlockTextures = mmAlloc(sizeof(BlockTexture) * 20, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:texanim"));
    bzero(gBlockTextures, sizeof(BlockTexture) * 20);
    D_800B97A8 = mmAlloc(sizeof(BlockTextureScroller) * MAX_TEXTURE_SCROLLERS, ALLOC_TAG_TRACK_COL, ALLOC_NAME("trk:texscroll"));
    bzero(D_800B97A8, sizeof(BlockTextureScroller) * MAX_TEXTURE_SCROLLERS);
    // @recomp: Use custom render list
    bzero(recomp_RenderList, sizeof(u32) * RECOMP_RENDER_LIST_LENGTH);
    recomp_RenderList[0] = -0x4000;
}

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
    block_color_table_add_block(block);
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

RECOMP_PATCH void block_emplace(Block *block, s32 id, s32 param_3, s32 globalMapIdx) {
    s32 slot;
    s8 *ptr;

    for (slot = 0; slot < gLoadedBlockCount; slot++) {
        if (gLoadedBlockIds[slot] == -1) {
            break;
        }
    }

    if (slot == gLoadedBlockCount) {
        gLoadedBlockCount++;
        if (gLoadedBlockCount == MAX_BLOCKS) {
            STUBBED_PRINTF("trackLoadBlockEnd: track block overrun\n");
        }
    }

    ptr = gBlockIndices[globalMapIdx];
    ptr[param_3] = slot;
    
    gLoadedBlocks[slot] = block;
    gLoadedBlockIds[slot] = id;
    gBlockRefCounts[slot] = 1;

    // @recomp: Skip interpolation for first render
    recomp_blockInterpStates[slot].skipInterpolation = TRUE;

    if (block->unk3E != 0) {
        block_compute_vertex_colors(block, 0, 0, 1);
    }

    func_80058F3C();
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
    Block* block;
    s32 temp_v1;
    s8 *var_s8;
    s32 temp_v0;
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
    s8 objVisibilities[MAX_VISIBLE_OBJECTS];
    Mtx* sp78;

    // @recomp: Check if frame interpolation is active. We'll skip block culling if it is
    _Bool frameInterpActive = recomp_is_frame_interp_active();

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
        for (temp_v1 = 0; temp_v1 < ARRAYCOUNT_S(sp130); temp_v1++) { sp130[temp_v1] = 0; }
        
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
                    block = NULL;
                } else {
                    block = gLoadedBlocks[temp_v0];
                    block->vtxFlags ^= 1;
                    if (sp130[temp_v1] == 0) {
                        continue;
                    }
                }
                // @recomp: Don't cull if frame interpolation is active
                if (temp_v0 < 0 || (!frameInterpActive && func_800451A0(temp_s1, var_s2, block) == 0)) {
                    continue;
                }
                D_800B97B8 = temp_s1 * BLOCKS_GRID_UNIT_F;
                D_800B97BC = var_s2 * BLOCKS_GRID_UNIT_F;
                block_calc_shape_visibility(block, temp_s1, var_s2, sp240);
                if (UINT_80092a98 & 0x8000) {
                    if (block->unk3E != 0) {
                        block_compute_vertex_colors(block, temp_s1, var_s2, 0);
                    }
                    if ((block->unk49 != 0) && (UINT_80092a98 & 0x100)) {
                        func_8001F4C0(block, temp_s1, var_s2);
                    }
                }
                // @recomp: Pass extra info to block_add_to_render_list so it knows the absolute coords of the block
                recomp_addBlockToRenderListGridInfo.x = temp_s1 + gMapCurrentStreamCoordsX;
                recomp_addBlockToRenderListGridInfo.z = var_s2 + gMapCurrentStreamCoordsZ;
                recomp_addBlockToRenderListGridInfo.layer = sp240;
                recomp_addBlockToRenderListGridInfo.blockIndex = temp_v0;
                block_add_to_render_list(block, D_800B97B8, D_800B97BC);
            }
        }
    }
    func_80043FD8(objVisibilities);
    draw_render_list(sp78, objVisibilities);
    dl_add_debug_info(gMainDL, 0, "track/track.c", 0x5B2);
    // @recomp: Tag projgfx matrices
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, PROJGFX_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_15_Projgfx->vtbl->func5(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, 2);
    gDLL_15_Projgfx->vtbl->func5(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, 1);
    // @recomp: Tag modgfx matrices
    gEXMatrixGroupSimpleNormalAuto(gMainDL++, MODGFX_MTX_GROUP_ID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    gDLL_14_Modgfx->vtbl->func11(objVisibilities);
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

RECOMP_PATCH void draw_render_list(Mtx* rspMtxs, s8* visibilities) {
    BlockShape* shape;
    Vtx_t *tempVtx;
    s32 shapeIdx;
    BlockTextureScroller* temp_v0_7;
    s32 i;
    BlocksTextureIndexData* temp_v0_4;
    u32 forceTexSet;
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
    s32 renderFlags;
    s32 frameOptions;
    Block* block;
    Object** sp8C;
    Object *obj;

    _Bool frameInterpActive = recomp_is_frame_interp_active();

    spC4 = -1;
    sp8C = get_world_objects(NULL, NULL);
    gDLL_57->vtbl->func2(&spE0, &spDC, &spD8, &spD4, &spD0, &spCC);
    for (i = 1; i < gRenderListLength; i++) {
        // @recomp: Use custom render list
        temp_t6 = shapeIdx = (recomp_RenderList[i] & 0x3F80) >> 7;
        if (recomp_RenderList[i] & 0x40) {
            obj = sp8C[temp_t6];
            func_800436DC(obj, visibilities[temp_t6]);
            spA3 = 0;
        } else {
            // @fake
            //if (i) {}
            // @recomp: Use custom render list
            temp_v1 = recomp_RenderList[i] & 0x3F;
            forceTexSet = FALSE;
            if (temp_v1 != spC4) {
                spA3 = -1;
                SHORT_800b51dc = -1;
                spC4 = temp_v1;
                UINT_800b51e0 = TEX_FRAME(0);
                spA4 = (temp_v1 * 2) + rspMtxs;
                block = gBlocksToDraw[temp_v1];
            }
            // @recomp: Tag block shape matrices (tag each uniquely since shapes can be rendered in any order, independently of the block)
            //          TODO: animated water is jittery
            RecompBlockGridInfo *gridInfo = &recomp_blocksToDrawGridCells[temp_v1];
            RecompBlockInterpState *blockInterpState = &recomp_blockInterpStates[gridInfo->blockIndex];
            // Note: Blocks can reused in different grid cells, so we need to identify the exact absolute cell,
            //       not just the block itself (the same block can be rendered in two or more locations on the same frame).
            // 200x200 global grid (arbitrary size), 5 layers, (max of) 1000 shapes
            s32 shapeMatrixGroupID = 
                ((gridInfo->x + 100) * 200 * 5 * 1000) + 
                ((gridInfo->z + 100)       * 5 * 1000) + 
                (gridInfo->layer               * 1000) + 
                temp_t6 + 
                BLOCK_SHAPE_MTX_GROUP_ID_START;
            if (blockInterpState->skipInterpolation) {
                gEXMatrixGroupSkipAll(gMainDL++, shapeMatrixGroupID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            } else {
                gEXMatrixGroupSimpleVerts(gMainDL++, shapeMatrixGroupID, G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
            }
            shape = &block->shapes[temp_t6];
            if (shape->flags & RENDER_UNK20000000) {
                if (spA3 != 2) {
                    gSPMatrix(gMainDL++, OS_K0_TO_PHYSICAL(&spA4[1]), G_MTX_MODELVIEW | G_MTX_LOAD);
                    spA3 = 2;
                }
            } else {
                if (spA3 != 1) {
                    gSPMatrix(gMainDL++, OS_K0_TO_PHYSICAL(&spA4[0]), G_MTX_MODELVIEW | G_MTX_LOAD);
                    spA3 = 1;
                }
            }
            if (shape->materialIndex == 0xFF) {
                tex0 = NULL;
            } else {
                tex0 = block->materials[shape->materialIndex].texture;
            }
            if (shape->flags & RENDER_UNK2000) {
                if (tex0->flags & (RENDER_COMPOSITE_BASE | RENDER_COMPOSITE_OVERLAY)) {
                    dl_set_prim_color(&gMainDL, 0xFF, 0xFF, 0xFF, 0xA0);
                } else {
                    dl_set_prim_color(&gMainDL, 0xFF, 0xFF, 0xFF, 0x64);
                }
            } else {
                if (shape->envColourMode == 0xFF) {
                    dl_set_prim_color(&gMainDL, spE0, spDC, spD8, 0xFF);
                } else if (shape->envColourMode == 0xFE) {
                    dl_set_prim_color(&gMainDL, 0xFF, 0xFF, 0xFF, 0xFF);
                } else if (shape->flags & (RENDER_UNK40000000 | RENDER_UNK4000000 | RENDER_UNK2000000 | RENDER_UNK800000 | RENDER_UNK400000)) {
                    dl_set_prim_color(&gMainDL, 0xFF, 0xFF, 0xFF, 0xFF);
                } else {
                    func_8001F848(&gMainDL);
                }
            }
            renderFlags = shape->flags;
            if (renderFlags & RENDER_SHAPE_ANIMATED) {
                temp_v0_4 = func_8004A284(block, shape->animatorID);
                if (temp_v0_4 != NULL) {
                    frameOptions = gBlockTextures[temp_v0_4->textureIndex].unk4 << 8;
                    renderFlags |= gBlockTextures[temp_v0_4->textureIndex].flags;
                } else {
                    frameOptions = 0;
                }
                if ((shape->animatorID != SHORT_800b51dc) || (frameOptions != UINT_800b51e0)) {
                    SHORT_800b51dc = shape->animatorID;
                    UINT_800b51e0 = frameOptions;
                    forceTexSet = TRUE;
                }
            } else {
                SHORT_800b51dc = -1;
                frameOptions = 0;
            }
            if (shape->blendMaterialIndex != 0xFF) {
                tex1 = block->materials[shape->blendMaterialIndex].texture;
            } else {
                tex1 = NULL;
            }
            tex_gdl_set_textures(&gMainDL, tex0, tex1, renderFlags, frameOptions, forceTexSet, FALSE);
            if (shape->unk16 != 0xFF) {
                temp_v0_7 = func_80049D68(shape->unk16);
                gDPSetTileSize(gMainDL++, 0, temp_v0_7->uOffsetA, temp_v0_7->vOffsetA, (tex0->width - 1) << 2, (tex0->height - 1) << 2);
                if (tex1 != NULL) {
                    gDPSetTileSize(gMainDL++, 1, temp_v0_7->uOffsetB, temp_v0_7->vOffsetB, (tex1->width - 1) << 2, (tex1->height - 1) << 2);
                }
            } else if ((tex0 != NULL) && (tex0->flags & (RENDER_COMPOSITE_BASE | RENDER_COMPOSITE_OVERLAY))) {
                gDPSetTileSize(gMainDL++, 0, 0, 0, (tex0->width - 1) << 2, (tex0->height - 1) << 2);
                if (tex1 != NULL) {
                    gDPSetTileSize(gMainDL++, 1, 0, 0, (tex1->width - 1) << 2, (tex1->height - 1) << 2);
                }
            }
            shapeIdx = (shape - block->shapes) * 3;
            gMainDL->words.w0 = block->gdlGroups[shapeIdx].words.w0;
            gMainDL->words.w1 = block->gdlGroups[shapeIdx].words.w1;
            // @recomp: We disable CPU backface culling so re-enable it for the RSP (RT64)
            if (frameInterpActive || !recomp_cpuBlockShapeCulling) {
                gMainDL->words.w1 |= G_CULL_BACK;
            }
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
            if ((renderFlags & (RENDER_FOG_ACTIVE | RENDER_DECAL_SIMPLE | RENDER_DECAL)) == (RENDER_FOG_ACTIVE | RENDER_DECAL_SIMPLE | RENDER_DECAL)) {
                temp_s0_2 = gMainDL - temp_s5;
                dl_set_geometry_mode(&gMainDL, G_FOG);
                if (renderFlags & (RENDER_UNK2000 | RENDER_SEMI_TRANSPARENT)) {
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
    // @recomp: Enable interpolation for blocks that were drawn
    for (i = 0; i < gBlocksToDrawIdx; i++) {
        RecompBlockGridInfo *gridInfo = &recomp_blocksToDrawGridCells[i];
        RecompBlockInterpState *blockInterpState = &recomp_blockInterpStates[gridInfo->blockIndex];
        blockInterpState->skipInterpolation = FALSE;
    }
}

RECOMP_PATCH void block_calc_shape_visibility(Block* block, s16 arg1, s16 arg2, s16 arg3) {
    BlockShape* shape;
    EncodedTri* triEnd;
    EncodedTri* tri;
    f32 pad[5];
    f32 Xmax;
    f32 Fx;
    f32 Fy;
    f32 Fz;
    f32 V2z;
    BlockShape* shapesEnd;
    f32 V1x;
    f32 Xmin;
    f32 Ymin;
    f32 Zmin;
    f32 V1y;
    f32 Ymax;
    f32 Zmax;
    f32 V2y;
    f32 V2x;
    f32 V1z;
    s32 Wx;
    s32 Wy;
    s32 Wz;
    s32 visible;
    u32 i;
    f32 dotProduct;
    //f32 Fd;
    BlockVertex  *shapeVtx;

    _Bool frameInterpActive = recomp_is_frame_interp_active();

    Wx = (D_800B51E4->tx - gWorldX) - D_800B97B8;
    Wy = D_800B51E4->ty;
    Wz = (D_800B51E4->tz - gWorldZ) - D_800B97BC;

    //Shape-level culling (based on bounds)
    shape = block->shapes;
    shapesEnd = &block->shapes[block->shapeCount];
    while ((u32) shape < (u32) shapesEnd) {
        //Check shape flags
        if (shape->flags & RENDER_SHAPE_HIDE) {
            shape->flags &= ~RENDER_SHAPE_VISIBLE;
            shape++;
            continue;
        }

        // @recomp: Skip if frame interpolation is active
        if (frameInterpActive) {
            // Re-enable all tris in case some were culled when frame interp was previously off
            tri = block->encodedTris;
            tri += shape->triBase;
            triEnd = block->encodedTris;
            triEnd += shape[1].triBase;

            while ((u32) tri < (u32) triEnd) {
                tri->d1 |= 1;
                tri++;
            }

            shape->flags |= RENDER_SHAPE_VISIBLE;
            shape++;
            continue;
        }

        if ((D_800B9794 != 0) && 
            ((D_800B979C & 1) || !(shape->flags & (RENDER_UNK2000 | RENDER_DECAL_SIMPLE | RENDER_SEMI_TRANSPARENT))) && 
            (func_80045600((shape - block->shapes), &D_800B9780, arg1, arg2, arg3) == 0)
        ) {
            shape->flags &= ~RENDER_SHAPE_VISIBLE;
            shape++;
            continue;
        }

        visible = TRUE;

        //Get shape's bounding box
        Xmax = ((shape->Xmax * 4) | SHAPE_BB_REMAINDER_X_MAX(shape)) + D_800B97B8;
        Xmin = ((shape->Xmin * 4) | SHAPE_BB_REMAINDER_X_MIN(shape)) + D_800B97B8;
        Ymin = shape->Ymin;
        Ymax = shape->Ymax;
        Zmax = ((shape->Zmax * 4) | SHAPE_BB_REMAINDER_Z_MAX(shape)) + D_800B97BC;
        Zmin = ((shape->Zmin * 4) | SHAPE_BB_REMAINDER_Z_MIN(shape)) + D_800B97BC;

        for (i = 0; i < ARRAYCOUNT_S(gFrustumPlanes); i++) {
            if (gFrustumPlanes[i].unk14[0] & 1) {
                V1x = Xmax;
                V2x = Xmin;
            } else {
                V1x = Xmin;
                V2x = Xmax;
            }

            if (gFrustumPlanes[i].unk14[0] & 2) {
                V1y = Ymax;
                V2y = Ymin;
            } else {
                V1y = Ymin;
                V2y = Ymax;
            }

            if (gFrustumPlanes[i].unk14[0] & 4) {
                V1z = Zmax;
                V2z = Zmin;
            } else {
                V1z = Zmin;
                V2z = Zmax;
            }

            //Fd = gFrustumPlanes[i].d; // not used but required to be loaded here
            Fx = gFrustumPlanes[i].x;
            Fy = gFrustumPlanes[i].y;
            Fz = gFrustumPlanes[i].z;

            dotProduct = gFrustumPlanes[i].d + ((V1x * Fx) + (V1y * Fy) + (V1z * Fz));
            if (dotProduct < 0.0f) {
                dotProduct = gFrustumPlanes[i].d + ((V2x * Fx) + (V2y * Fy) + (V2z * Fz));
                if (dotProduct < 0.0f) {
                    visible = FALSE;
                    break;
                }
            }
        }

        //Set flags
        if (visible == FALSE) {
            shape->flags &= ~RENDER_SHAPE_VISIBLE;
            shape++;
            continue;
        }

        //Tri-level culling (based on vertex coords)
        if (!(shape->flags & RENDER_NO_CULL)) {
            visible = FALSE;
            shapeVtx = &block->vertices[shape->vtxBase];
            tri = block->encodedTris;
            tri += shape->triBase;
            triEnd = block->encodedTris;
            triEnd += shape[1].triBase;

            if (recomp_cpuBlockShapeCulling) {
                while ((u32) tri < (u32) triEnd) {
                    s32 a2 = (s32) tri->d0 >> 0xD;
                    s32 Ax, Ay, Az;
                    s32 Bx, By, Bz;

                    Ax = shapeVtx[a2 & 0x1F].ob[0] - Wx;
                    Ay = shapeVtx[a2 & 0x1F].ob[1] - Wy;
                    Az = shapeVtx[a2 & 0x1F].ob[2] - Wz;
                    Bx = (s32) tri->d0 >> 0x12;
                    By = (s32) (tri->d1 << 0xE) >> 0x12;
                    Bz = (s32) tri->d1 >> 0x12;

                    //Get dot product of vectors (check if seeing back of face?)
                    if ((Ax * Bx) + (Ay * By) + (Az * Bz) < 0) {
                        tri->d1 |= 1;
                        visible = TRUE;
                    } else {
                        tri->d1 &= ~1;
                    }

                    tri++;
                }
            } else {
                // @recomp: Disable CPU backface culling. This casues re-used blocks to hide triangles in some spots
                //          (notably a problem in CapeClaw with the many re-used ocean blocks). We'll switch on
                //          the backface culling geometry mode later to compensate.
                while ((u32) tri < (u32) triEnd) {
                    tri->d1 |= 1;
                    tri++;
                }
                visible = TRUE;
            }

            if (visible == FALSE) {
                shape->flags &= ~RENDER_SHAPE_VISIBLE;
                shape++;
                continue;
            }
        }

        shape->flags |= RENDER_SHAPE_VISIBLE;
        shape++;
    }
}

RECOMP_PATCH void block_add_to_render_list(Block *block, f32 x, f32 z) {
    s32 unused;
    s32 oldRenderListLength;
    s32 i;
    s32 param;
    s32 pad;
    MtxF mf;
    MtxF mf2;
    
    oldRenderListLength = gRenderListLength;

    for (i = 0; i < block->shapeCount; i++) {
        // @recomp: Use new render list max length
        if ((block->shapes[i].flags & RENDER_SHAPE_VISIBLE) && gRenderListLength < RECOMP_RENDER_LIST_LENGTH) {
            if (block->shapes[i].flags & RENDER_SEMI_TRANSPARENT) {
                param = 100000 - (gBlocksToDrawIdx * 400) - i;

                if (block->shapes[i].flags & RENDER_UNK2000) {
                    param -= 200;
                }
            } else {
                param = 200000 - (gBlocksToDrawIdx * 400) - i;
            }

            // @recomp: Use custom render list
            recomp_RenderList[gRenderListLength] = (param << 14) | (i << 7) | gBlocksToDrawIdx;
            gRenderListLength++;
        }
    }

    if (((oldRenderListLength & 0xFFFFFFFF) != (u32) gRenderListLength) && gBlocksToDrawIdx < MAX_BLOCKS) {
        // @recomp: Record info for tagging matrices later
        RecompBlockGridInfo *gridInfo = &recomp_blocksToDrawGridCells[gBlocksToDrawIdx];
        gridInfo->x = recomp_addBlockToRenderListGridInfo.x;
        gridInfo->z = recomp_addBlockToRenderListGridInfo.z;
        gridInfo->layer = recomp_addBlockToRenderListGridInfo.layer;
        gridInfo->blockIndex = recomp_addBlockToRenderListGridInfo.blockIndex;

        gBlocksToDraw[gBlocksToDrawIdx] = block;
        gBlocksToDrawIdx++;

        matrix_translation(&mf, x, 0.0f, z);
        matrix_f2l_4x3(&mf, gWorldRSPMatrices);

        gWorldRSPMatrices++;

        mf.m[3][1] = block->minY;

        matrix_scaling(&mf2, 1.0f, 0.05f, 1.0f);
        matrix_concat(&mf2, &mf, &mf);
        matrix_f2l_4x3(&mf, gWorldRSPMatrices);

        gWorldRSPMatrices++;
    }
}

RECOMP_PATCH void func_80043FD8(s8* objVisibilities) {
    Object* object;
    Object** objects;
    s32 numObjs;
    s32 visibleStartIdx;
    s32 i;
    s32 var_v0;
    s8* vis;

    objects = get_world_objects(NULL, NULL);
    // Separate invisible objects from visible objects
    visibleStartIdx = obj_visibility_sort_objects(&numObjs);
    if (numObjs > MAX_VISIBLE_OBJECTS) {
        // @recomp: Restore print
        recomp_eprintf("depthSortObjects: MAX_VISIBLE_OBJECTS exceeded\n");
        numObjs = MAX_VISIBLE_OBJECTS;
    }
    // Depth sort just the visible objects
    obj_depth_sort_objects(visibleStartIdx, numObjs - 1);
    for (i = 0; i < numObjs; i++) {
        object = objects[i];
        vis = &objVisibilities[i];
        if (i < visibleStartIdx) {
            // Object is always invisible due to its definition
            *vis = FALSE;
        } else {
            *vis = track_obj_vis_check(object);
            if (*vis && (object->shadow != NULL) && (object->def->shadowType == OBJ_SHADOW_GEOM)) {
                shadows_update_obj_geom(object, 0, 0, gUpdateRate);
            }
            if ((object->shadow != NULL) && (object->def->shadowType == OBJ_SHADOW_BOX))  {
                shadows_update_obj_box(object);
            }
            // @recomp: Use new render list max length
            if (gRenderListLength < RECOMP_RENDER_LIST_LENGTH) {
                if (object->def->flags & OBJDEF_FORCE_OPAQUE_DRAW_ORDER) {
                    var_v0 = 150000 - i;
                } else if ((object->opacityWithFade == 0xFF) && !(object->srt.flags & OBJFLAG_FORCE_TRANSPARENT_DRAW_ORDER)) {
                    var_v0 = 150000 - i;
                } else {
                    var_v0 = i + 50000;
                }
                // @recomp: Use custom render list
                recomp_RenderList[gRenderListLength] = (var_v0 << 14) | (i << 7) | 0x40;
                gRenderListLength += 1;
            }
            /* default.dol
            else {
                STUBBED_PRINTF("DrawGroups overflow\n"); // (default.dol)
            }
            */
        }
    }
    if (gRenderListLength >= 2) {
        // @recomp: Use custom render list
        func_800441F4(recomp_RenderList, gRenderListLength);
    }
}

RECOMP_PATCH void func_800436DC(Object* obj, s32 visibility) {
    s8 sp37;
    u8 someBool;

    // @recomp: Get base matrix tagging group
    _Bool skipInterp;
    u32 objMtxGroup = recomp_obj_get_matrix_group(obj, &skipInterp);

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
    if (skipInterp) {
        gEXMatrixGroupSkipAll(gMainDL++, objMtxGroup + OBJ_MODGFX_MTX_GROUP_ID_START, 
            G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    } else {
        gEXMatrixGroupDecomposedVertsOrderAuto(gMainDL++, objMtxGroup + OBJ_MODGFX_MTX_GROUP_ID_START, 
            G_EX_NOPUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    }
    gDLL_14_Modgfx->vtbl->func6(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, 1, obj);
    if (sp37 >= 2) {
        if ((obj->id != OBJ_IMSnowBike) && (obj->id != OBJ_CRSnowBike)) {
            gDLL_13_Expgfx->vtbl->func6(obj, &gMainDL, &gWorldRSPMatrices, &D_800B51D4, 1, 0, 0);
        }
    }
    objprint_func(&gMainDL, &gWorldRSPMatrices, &D_800B51D4, &D_800B51D8, obj, visibility);
    if (sp37 != 0) {
        if ((obj->id != OBJ_IMSnowBike) && (obj->id != OBJ_CRSnowBike)) {
            gDLL_13_Expgfx->vtbl->func6(obj, &gMainDL, &gWorldRSPMatrices, &D_800B51D4, 0, 0, 0);
        }
    }
    if ((obj->linkedObject != NULL) && (visibility != 0)) {
        sp37 = gDLL_13_Expgfx->vtbl->func10(obj->linkedObject);
        if (sp37 >= 2) {
            gDLL_13_Expgfx->vtbl->func6(obj->linkedObject, &gMainDL, &gWorldRSPMatrices, &D_800B51D4, 1, 0, 0);
        }
        if (sp37 != 0) {
            gDLL_13_Expgfx->vtbl->func6(obj->linkedObject, &gMainDL, &gWorldRSPMatrices, &D_800B51D4, 0, 0, 0);
        }
    }
}
