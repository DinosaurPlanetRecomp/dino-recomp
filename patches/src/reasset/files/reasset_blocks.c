#include "reasset_blocks.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_iterator.h"
#include "reasset/files/reasset_textures.h"
#include "reasset/list.h"
#include "reasset/buffer.h"
#include "reasset/bin_ptr.h"

#include "PR/ultratypes.h"
#include "sys/fs.h"
#include "sys/map.h"
#include "sys/memory.h"
#include "sys/rarezip.h"

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    Buffer hit;
    BinPtr hitPtr;
    _Bool delete;
} HitEntry;

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    Buffer block;
    BinPtr blockPtr;
    struct {
        List list; // list[HitEntry]
        U32List idList; // list[ReAssetID]
        U32ValueHashmapHandle map; // ReAssetID -> hits list index
        s32 originalCount;
    } hits;
} BlockEntry;

typedef struct {
    ReAssetID id;
    ReAssetNamespace owner;
    struct {
        List list; // list[BlockEntry]
        U32List idList; // list[ReAssetID]
        U32ValueHashmapHandle map; // ReAssetID -> blocks list index
        s32 originalCount;
    } blocks;
} TrkBlkEntry;

static s32 trkblkOriginalCount;
static List trkblkList; // list[TrkBlkEntry]
static U32List trkblkIDList; // list[ReAssetID]
static U32ValueHashmapHandle trkblkMap; // ReAssetID -> trkblk list index

typedef struct {
    ReAssetResolveMap resolveMap;
    U32ValueHashmapHandle hitResolveMaps; // ReAssetID (block) -> ReAssetResolveMap
} BlockResolveMap;

static struct {
    ReAssetResolveMap resolveMap;
    U32MemoryHashmapHandle blockResolveMaps; // ReAssetID (trkblk) -> BlockResolveMap
} trkblkResolveMap;

static void create_hit_resolve_map(BlockResolveMap *block, ReAssetID blockID) {
    if (recomputil_u32_value_hashmap_contains(block->hitResolveMaps, blockID)) {
        // Already exists
        return;
    }

    ReAssetIDData *idData = reasset_id_lookup_data(blockID);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);

    const char *resolveMapName = reasset_alloc_sprintf("Hit (Block %s:%d)", namespaceName, idData->identifier);
    
    recomputil_u32_value_hashmap_insert(block->hitResolveMaps, blockID, reasset_resolve_map_create(resolveMapName));
}

static void create_block_resolve_map(ReAssetID trkblkID) {
    if (!recomputil_u32_memory_hashmap_create(trkblkResolveMap.blockResolveMaps, trkblkID)) {
        // Already exists
        return;
    }

    ReAssetIDData *idData = reasset_id_lookup_data(trkblkID);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);

    const char *resolveMapName = reasset_alloc_sprintf("Block (TrkBlk %s:%d)", namespaceName, idData->identifier);

    BlockResolveMap *blockResolveMap = recomputil_u32_memory_hashmap_get(trkblkResolveMap.blockResolveMaps, trkblkID);
    reasset_assert(blockResolveMap != NULL, "[reasset] bug! create_block_resolve_map block resolve map get failed.");

    blockResolveMap->resolveMap = reasset_resolve_map_create(resolveMapName);
    blockResolveMap->hitResolveMaps = recomputil_create_u32_value_hashmap();
}

static void hit_list_element_free(void *element) {
    HitEntry *entry = element;
    buffer_free(&entry->hit);
}

static void block_list_element_free(void *element) {
    BlockEntry *entry = element;
    buffer_free(&entry->block);
    list_free(&entry->hits.list);
    u32list_free(&entry->hits.idList);
    recomputil_destroy_u32_value_hashmap(entry->hits.map);
}

static void trkblk_list_element_free(void *element) {
    TrkBlkEntry *entry = element;
    list_free(&entry->blocks.list);
    u32list_free(&entry->blocks.idList);
    recomputil_destroy_u32_value_hashmap(entry->blocks.map);
}

static HitEntry* get_hit(BlockEntry *block, ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(block->hits.map, id, &listIdx)) {
        return list_get(&block->hits.list, listIdx);
    }

    return NULL;
}

static HitEntry* get_or_create_hit(BlockEntry *block, ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(block->hits.map, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < block->hits.originalCount, 
                "[reasset] Attempted to patch out-of-bounds base hit: %d", idData->identifier);
        }

        u32list_add(&block->hits.idList, id);

        // Create hit entry
        listIdx = list_get_length(&block->hits.list);
        
        HitEntry *entry = list_add(&block->hits.list);
        entry->id = id;
        entry->owner = idData->namespace;

        buffer_init(&entry->hit, 0);

        recomputil_u32_value_hashmap_insert(block->hits.map, id, listIdx);
    }

    return list_get(&block->hits.list, listIdx);
}

static BlockEntry* get_block(TrkBlkEntry *trkblk, ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(trkblk->blocks.map, id, &listIdx)) {
        return list_get(&trkblk->blocks.list, listIdx);
    }

    return NULL;
}

static BlockEntry* get_or_create_block(TrkBlkEntry *trkblk, ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(trkblk->blocks.map, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < trkblk->blocks.originalCount, 
                "[reasset] Attempted to patch out-of-bounds base block: %d", idData->identifier);
        }

        u32list_add(&trkblk->blocks.idList, id);

        // Create block entry
        listIdx = list_get_length(&trkblk->blocks.list);
        
        BlockEntry *entry = list_add(&trkblk->blocks.list);
        entry->id = id;
        entry->owner = idData->namespace;

        buffer_init(&entry->block, 0);

        list_init(&entry->hits.list, sizeof(BlockEntry), 0);
        u32list_init(&entry->hits.idList, 0);
        list_set_element_free_callback(&entry->hits.list, hit_list_element_free);
        entry->hits.map = recomputil_create_u32_value_hashmap();

        recomputil_u32_value_hashmap_insert(trkblk->blocks.map, id, listIdx);

        // Create resolve map for hits (for this block)
        BlockResolveMap *blockResolveMap = recomputil_u32_memory_hashmap_get(trkblkResolveMap.blockResolveMaps, trkblk->id);
        reasset_assert(blockResolveMap != NULL, "[reasset] bug! get_or_create_block block resolve map get failed.");
        create_hit_resolve_map(blockResolveMap, id);
    }

    return list_get(&trkblk->blocks.list, listIdx);
}

static TrkBlkEntry* get_trkblk(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(trkblkMap, id, &listIdx)) {
        return list_get(&trkblkList, listIdx);
    }

    return NULL;
}

static TrkBlkEntry* get_or_create_trkblk(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(trkblkMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < trkblkOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base trkblk: %d", idData->identifier);
        }

        u32list_add(&trkblkIDList, id);

        // Create trkblk entry
        listIdx = list_get_length(&trkblkList);
        
        TrkBlkEntry *entry = list_add(&trkblkList);
        entry->id = id;
        entry->owner = idData->namespace;

        list_init(&entry->blocks.list, sizeof(BlockEntry), 0);
        u32list_init(&entry->blocks.idList, 0);
        list_set_element_free_callback(&entry->blocks.list, block_list_element_free);
        entry->blocks.map = recomputil_create_u32_value_hashmap();

        recomputil_u32_value_hashmap_insert(trkblkMap, id, listIdx);

        // Create resolve map for blocks (for this trkblk)
        create_block_resolve_map(id);
    }

    return list_get(&trkblkList, listIdx);
}

void reasset_blocks_init(void) {
    // Init
    trkblkOriginalCount = (reasset_fst_get_file_size(TRKBLK_BIN) / sizeof(u16)) - 2;
    u16 *trkblkOriginal = reasset_fst_alloc_load_file(TRKBLK_BIN, NULL);

    list_init(&trkblkList, sizeof(TrkBlkEntry), trkblkOriginalCount);
    u32list_init(&trkblkIDList, trkblkOriginalCount);
    list_set_element_free_callback(&trkblkList, trkblk_list_element_free);
    trkblkMap = recomputil_create_u32_value_hashmap();
    trkblkResolveMap.resolveMap = reasset_resolve_map_create("TrkBlk");
    trkblkResolveMap.blockResolveMaps = recomputil_create_u32_memory_hashmap(sizeof(BlockResolveMap));

    s32 *blocksOriginalTab = reasset_fst_alloc_load_file(BLOCKS_TAB, NULL);
    s32 *hitsOriginalTab = reasset_fst_alloc_load_file(HITS_TAB, NULL);

    // Add original trkblk, blocks, and hits (preserving original order)
    for (s32 trkblkIdx = 0; trkblkIdx < trkblkOriginalCount; trkblkIdx++) {
        ReAssetID trkblkID = reasset_base_id(trkblkIdx);
        TrkBlkEntry *trkblkEntry = get_or_create_trkblk(trkblkID);

        // Blocks
        s32 blockStart = trkblkOriginal[trkblkIdx];
        s32 blockEnd = trkblkOriginal[trkblkIdx + 1];

        trkblkEntry->blocks.originalCount = blockEnd - blockStart;
        
        for (s32 blkIdx = blockStart; blkIdx < blockEnd; blkIdx++) {
            ReAssetID blockID = reasset_base_id(blkIdx - blockStart);
            BlockEntry *block = get_or_create_block(trkblkEntry, blockID);
            
            s32 blockOffset = blocksOriginalTab[blkIdx];
            s32 blockSize = blocksOriginalTab[blkIdx + 1] - blockOffset;

            buffer_set_base(&block->block, BLOCKS_BIN, blockOffset, blockSize);

            // Hits
            s32 hitsOffset = hitsOriginalTab[blkIdx];
            s32 hitsSize = hitsOriginalTab[blkIdx + 1] - hitsOffset;
            s32 numHits = hitsSize / sizeof(HitsLine);

            block->hits.originalCount = numHits;

            for (s32 hitIdx = 0; hitIdx < numHits; hitIdx++) {
                ReAssetID hitID = reasset_base_id(hitIdx);
                HitEntry *hit = get_or_create_hit(block, hitID);

                buffer_set_base(&hit->hit, HITS_BIN, hitsOffset + (hitIdx * sizeof(HitsLine)), sizeof(HitsLine));
            }
        }
    }

    // Clean up
    recomp_free(hitsOriginalTab);
    recomp_free(blocksOriginalTab);
    recomp_free(trkblkOriginal);
}

static void block_decompress(Buffer *buffer) {
    u32 size;
    void *data = buffer_get(buffer, &size);
    if (data == NULL || size < 0x9) {
        // Invalid block buffer
        return;
    }

    s32 decompressedSize = rarezip_uncompress_size((u8*)data + 4);
    if (decompressedSize <= 0) {
        // Already decompressed or zero size
        return;
    }

    // header + gzip header - 1 + data
    u32 newDataOffset = 4 + 4; // Note: Must be 4-byte aligned
    u32 newSize = newDataOffset + decompressedSize;
    void *newData = recomp_alloc(newSize);
    bzero(newData, newSize);

    bcopy(data, newData, 4); // header
    *((s32*)((u8*)newData + 0x4)) = -1; // decompressedSize
    rarezip_uncompress((u8*)data + 4, (u8*)newData + newDataOffset, decompressedSize);

    buffer_set(buffer, newData, newSize);

    recomp_free(newData);
}

void reasset_blocks_repack(void) {
    s32 newTrkblkCount = list_get_length(&trkblkList);

    s32 abs = 0;
    for (s32 i = 0; i < newTrkblkCount; i++) {
        TrkBlkEntry *trkblkEntry = list_get(&trkblkList, i);
        
        s32 newBlocksCount = list_get_length(&trkblkEntry->blocks.list);
        for (s32 k = 0; k < newBlocksCount; k++, abs++) {
            BlockEntry *block = list_get(&trkblkEntry->blocks.list, k);

            if (block->owner != REASSET_BASE_NAMESPACE) {
                // For any block that needs to be patched (those not owned by base), 
                // we need them to be uncompressed.
                block_decompress(&block->block);
            }
        }
    }

    // Calculate new sizes
    u32 trkblkBinSize = (newTrkblkCount + 2) * sizeof(u16);

    u32 blocksBinSize = 0;
    s32 totalNewBlocks = 0;
    u32 hitsBinSize = 0;
    s32 totalNewHits = 0;

    for (s32 i = 0; i < newTrkblkCount; i++) {
        TrkBlkEntry *trkblkEntry = list_get(&trkblkList, i);
        
        s32 newBlocksCount = list_get_length(&trkblkEntry->blocks.list);
        totalNewBlocks += newBlocksCount;

        for (s32 k = 0; k < newBlocksCount; k++) {
            BlockEntry *block = list_get(&trkblkEntry->blocks.list, k);

            blocksBinSize += buffer_get_size(&block->block);
            blocksBinSize = mmAlign2(blocksBinSize);

            s32 newHitsCount = list_get_length(&block->hits.list);
            s32 deletedHitsCount = 0;

            for (s32 j = 0; j < newHitsCount; j++) {
                HitEntry *hit = list_get(&block->hits.list, j);
                if (hit->delete) {
                    deletedHitsCount++;
                }
            }

            newHitsCount -= deletedHitsCount;
            totalNewHits += newHitsCount;
            hitsBinSize += newHitsCount * sizeof(HitsLine);
        }
    }

    u32 blocksTabSize = (totalNewBlocks + 2) * sizeof(s32);
    u32 hitsTabSize = (totalNewHits + 1) * sizeof(s32);

    // Alloc new files
    u16 *trkblkBin = recomp_alloc(trkblkBinSize);
    s32 *blocksTab = recomp_alloc(blocksTabSize);
    void *blocksBin = recomp_alloc(blocksBinSize);
    s32 *hitsTab = recomp_alloc(hitsTabSize);
    void *hitsBin = recomp_alloc(hitsBinSize);

    bzero(blocksBin, blocksBinSize);
    bzero(hitsBin, hitsBinSize);

    // Rebuild
    s32 blocksBinOffset = 0;
    s32 hitsBinOffset = 0;
    s32 absoluteBlockIdx = 0;

    for (s32 trkblkIdx = 0; trkblkIdx < newTrkblkCount; trkblkIdx++) {
        // TrkBlk
        TrkBlkEntry *trkblkEntry = list_get(&trkblkList, trkblkIdx);
        s32 trkblkIdentifier;
        const char *trkblkNamespace;
        reasset_id_lookup_name(trkblkEntry->id, &trkblkNamespace, &trkblkIdentifier);
        ReAssetIDData *trkblkIDData = reasset_id_lookup_data(trkblkEntry->id);

        if (trkblkIDData->namespace != REASSET_BASE_NAMESPACE) {
            reasset_log_debug("[reasset] New trkblk entry: %s:%d\n", trkblkNamespace, trkblkIdentifier);
        }

        trkblkBin[trkblkIdx] = absoluteBlockIdx;
        
        reasset_resolve_map_resolve_id(trkblkResolveMap.resolveMap, trkblkEntry->id, trkblkEntry->owner, trkblkIdx);

        BlockResolveMap *blockResolveMap = recomputil_u32_memory_hashmap_get(trkblkResolveMap.blockResolveMaps, trkblkEntry->id);
        reasset_assert(blockResolveMap != NULL, "[reasset] bug! reasset_blocks_repack block resolve map get failed.");

        s32 newBlocksCount = list_get_length(&trkblkEntry->blocks.list);
        for (s32 blockIdx = 0; blockIdx < newBlocksCount; blockIdx++) {
            // Block
            BlockEntry *block = list_get(&trkblkEntry->blocks.list, blockIdx);
            s32 blockIdentifier;
            const char *blockNamespace;
            reasset_id_lookup_name(block->id, &blockNamespace, &blockIdentifier);
            ReAssetIDData *blockIDData = reasset_id_lookup_data(block->id);

            if (blockIDData->namespace != REASSET_BASE_NAMESPACE) {
                reasset_log_debug("[reasset] New block: %s:%d[%s:%d]\n", 
                    trkblkNamespace, trkblkIdentifier,
                    blockNamespace, blockIdentifier);
            }

            reasset_resolve_map_resolve_id(blockResolveMap->resolveMap, block->id, block->owner, blockIdx);

            ReAssetResolveMap hitResolveMap;
            reasset_assert(recomputil_u32_value_hashmap_get(blockResolveMap->hitResolveMaps, block->id, &hitResolveMap),
                "[reasset] bug! reasset_blocks_repack hit resolve map get failed.");

            blocksTab[absoluteBlockIdx] = blocksBinOffset;
            bin_ptr_set(&block->blockPtr, blocksBin, blocksBinOffset, buffer_get_size(&block->block));
            buffer_copy_to_bin_ptr(&block->block, &block->blockPtr);
            blocksBinOffset += buffer_get_size(&block->block);
            blocksBinOffset = mmAlign2(blocksBinOffset);

            hitsTab[absoluteBlockIdx] = hitsBinOffset;

            s32 newHitsCount = list_get_length(&block->hits.list);
            for (s32 hitIdx = 0; hitIdx < newHitsCount; hitIdx++) {
                // Hit
                HitEntry *hit = list_get(&block->hits.list, hitIdx);
                if (hit->delete) {
                    s32 hitIdentifier;
                    const char *hitNamespace;
                    reasset_id_lookup_name(hit->id, &hitNamespace, &hitIdentifier);

                    reasset_log_debug("[reasset] Deleted hit %s:%d[%s:%d][%s:%d]\n", 
                        trkblkNamespace, trkblkIdentifier,
                        blockNamespace, blockIdentifier,
                        hitNamespace, hitIdentifier);
                    continue;
                }

                reasset_resolve_map_resolve_id(hitResolveMap, hit->id, hit->owner, hitIdx);
                
                bin_ptr_set(&hit->hitPtr, hitsBin, hitsBinOffset, sizeof(HitsLine));
                buffer_copy_to_bin_ptr(&hit->hit, &hit->hitPtr);
                hitsBinOffset += sizeof(HitsLine);
            }

            absoluteBlockIdx++;
        }
    }

    // Terminate files
    trkblkBin[newTrkblkCount] = absoluteBlockIdx;
    trkblkBin[newTrkblkCount + 1] = -1;
    blocksTab[totalNewBlocks] = blocksBinOffset;
    blocksTab[totalNewBlocks + 1] = -1;
    hitsTab[totalNewBlocks] = hitsBinOffset;

    reasset_assert((u32)blocksBinOffset <= blocksBinSize, 
        "[reasset] Overflow writing BLOCKS.bin. %d > %d", blocksBinOffset, blocksBinSize);
    reasset_assert((u32)hitsBinOffset <= hitsBinSize, 
        "[reasset] Overflow writing HITS.bin. %d > %d", hitsBinOffset, hitsBinSize);

    // Finalize resolve maps
    for (s32 trkblkIdx = 0; trkblkIdx < newTrkblkCount; trkblkIdx++) {
        TrkBlkEntry *trkblkEntry = list_get(&trkblkList, trkblkIdx);

        BlockResolveMap *blockResolveMap = recomputil_u32_memory_hashmap_get(trkblkResolveMap.blockResolveMaps, trkblkEntry->id);
        reasset_assert(blockResolveMap != NULL, "[reasset] bug! reasset_blocks_repack block resolve map get failed.");

        s32 newBlocksCount = list_get_length(&trkblkEntry->blocks.list);
        for (s32 blockIdx = 0; blockIdx < newBlocksCount; blockIdx++) {
            BlockEntry *block = list_get(&trkblkEntry->blocks.list, blockIdx);

            ReAssetResolveMap hitResolveMap;
            reasset_assert(recomputil_u32_value_hashmap_get(blockResolveMap->hitResolveMaps, block->id, &hitResolveMap),
                "[reasset] bug! reasset_blocks_repack hit resolve map get failed.");

            reasset_resolve_map_finalize(hitResolveMap);
        }

        reasset_resolve_map_finalize(blockResolveMap->resolveMap);
    }

    reasset_resolve_map_finalize(trkblkResolveMap.resolveMap);

    // Set new files
    reasset_fst_set_internal(TRKBLK_BIN, trkblkBin, trkblkBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log_info("[reasset] Rebuilt TRKBLK.bin (count: %d).\n", newTrkblkCount);
    reasset_fst_set_internal(BLOCKS_TAB, blocksTab, blocksTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(BLOCKS_BIN, blocksBin, blocksBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log_info("[reasset] Rebuilt BLOCKS.tab & BLOCKS.bin (count: %d, bin size: 0x%X).\n", totalNewBlocks, blocksBinSize);
    reasset_fst_set_internal(HITS_TAB, hitsTab, hitsTabSize, /*ownedByReAsset=*/TRUE);
    reasset_fst_set_internal(HITS_BIN, hitsBin, hitsBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log_info("[reasset] Rebuilt HITS.tab & HITS.bin (count: %d, bin size: 0x%X).\n", totalNewBlocks, hitsBinSize);
}

void reasset_blocks_patch(void) {
    ReAssetResolveMap tex1ResolveMap = reasset_textures_get_resolve_map(TEX1);

    s32 trkblkCount = list_get_length(&trkblkList);

    for (s32 trkblkIdx = 0; trkblkIdx < trkblkCount; trkblkIdx++) {
        TrkBlkEntry *trkblkEntry = list_get(&trkblkList, trkblkIdx);

        const char *trkblkNamespaceName;
        s32 trkblkIdentifier;
        reasset_id_lookup_name(trkblkEntry->id, &trkblkNamespaceName, &trkblkIdentifier);

        s32 blocksCount = list_get_length(&trkblkEntry->blocks.list);
        for (s32 blockIdx = 0; blockIdx < blocksCount; blockIdx++) {
            BlockEntry *blockEntry = list_get(&trkblkEntry->blocks.list, blockIdx);
            if (blockEntry->owner == REASSET_BASE_NAMESPACE) {
                continue;
            }

            const char *blockNamespaceName;
            s32 blockIdentifier;
            reasset_id_lookup_name(blockEntry->id, &blockNamespaceName, &blockIdentifier);

            void *blockBin = blockEntry->blockPtr.ptr;
            if (blockBin != NULL) {
                reasset_assert(rarezip_uncompress_size((u8*)blockBin + 4) == -1,
                    "[reasset] bug! Block needs to be uncompressed for the patch stage.");
                
                Block *block = (Block*)((u8*)blockBin + 0x8);

                // Patch texture IDs
                BlocksMaterial *materials = (BlocksMaterial*)((u32)block->materials + (u32)block);
                for (s32 k = 0; k < block->materialCount; k++) {
                    s32 *texIDPtr = (s32*)&materials[k].texture;
                    s32 texID = *texIDPtr;

                    s32 resolvedID = reasset_resolve_map_lookup(tex1ResolveMap, reasset_id(blockEntry->owner, texID));
                    if (resolvedID != -1) {
                        *texIDPtr = resolvedID;
                    } else if (!reasset_textures_is_base_id(TEX1, texID)) {
                        *texIDPtr = 16; // fallback texture to make it obvious it's a bad texture

                        reasset_log_warning("[reasset] WARN: Failed to patch block (%s:%d[%s:%d]) texture %d ID 0x%X. Texture was not defined!\n",
                            trkblkNamespaceName, trkblkIdentifier,
                            blockNamespaceName, blockIdentifier, 
                            k, texID);
                    }
                }
            }
        }
    }
}

void reasset_blocks_cleanup(void) {
    list_free(&trkblkList);
    u32list_free(&trkblkIDList);
    recomputil_destroy_u32_value_hashmap(trkblkMap);
}

// MARK: TrkBlk

RECOMP_EXPORT ReAssetIterator reasset_trkblk_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_trkblk_create_iterator");

    return reasset_iterator_create(&trkblkIDList);
}

RECOMP_EXPORT void reasset_trkblk_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_trkblk_link");

    reasset_resolve_map_link(trkblkResolveMap.resolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_trkblk_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_trkblk_get_resolve_map");

    return trkblkResolveMap.resolveMap;
}

_Bool reasset_trkblk_is_base_id(s32 id) {
    return id >= 0 && id < trkblkOriginalCount;
}

// MARK: Blocks

RECOMP_EXPORT void reasset_blocks_set(ReAssetID trkblkID, ReAssetID id, ReAssetNamespace owner, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_blocks_set");

    TrkBlkEntry *trkblk = get_or_create_trkblk(trkblkID);

    BlockEntry *entry = get_or_create_block(trkblk, id);
    buffer_set(&entry->block, data, sizeBytes);
    entry->owner = owner;

    const char *trkblkNamespaceName;
    s32 trkblkIdentifier;
    reasset_id_lookup_name(trkblkID, &trkblkNamespaceName, &trkblkIdentifier);
    const char *namespaceName;
    s32 identifier;
    reasset_id_lookup_name(id, &namespaceName, &identifier);
    reasset_log_debug("[reasset] Block set: %s:%d[%s:%d]\n", 
        trkblkNamespaceName, trkblkIdentifier,
        namespaceName, identifier);
}

RECOMP_EXPORT void* reasset_blocks_get(ReAssetID trkblkID, ReAssetID id, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_blocks_get");

    TrkBlkEntry *trkblk = get_trkblk(trkblkID);
    if (trkblk != NULL) {
        BlockEntry *entry = get_block(trkblk, id);
        if (entry != NULL) {
            if (reassetStage == REASSET_STAGE_RESOLVE) {
                return bin_ptr_get(&entry->blockPtr, outSizeBytes);
            } else {
                return buffer_get(&entry->block, outSizeBytes);
            }
        }
    }

    if (outSizeBytes != NULL) {
        *outSizeBytes = 0;
    }
    return NULL;
}

RECOMP_EXPORT ReAssetIterator reasset_blocks_create_iterator(ReAssetID trkblkID) {
    reasset_assert_stage_iterator_call("reasset_blocks_create_iterator");

    TrkBlkEntry *trkblk = get_trkblk(trkblkID);
    if (trkblk == NULL) {
        return reasset_iterator_create(NULL);
    }

    return reasset_iterator_create(&trkblk->blocks.idList);
}

RECOMP_EXPORT void reasset_blocks_link(ReAssetID trkblkID, ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_blocks_link");

    create_block_resolve_map(trkblkID);

    BlockResolveMap *blockResolveMap = recomputil_u32_memory_hashmap_get(trkblkResolveMap.blockResolveMaps, trkblkID);
    reasset_assert(blockResolveMap != NULL, "[reasset] bug! reasset_blocks_link block resolve map get failed.");

    reasset_resolve_map_link(blockResolveMap->resolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_blocks_get_resolve_map(ReAssetID trkblkID) {
    reasset_assert_stage_get_resolve_map_call("reasset_blocks_get_resolve_map");

    create_block_resolve_map(trkblkID);
    
    BlockResolveMap *blockResolveMap = recomputil_u32_memory_hashmap_get(trkblkResolveMap.blockResolveMaps, trkblkID);
    reasset_assert(blockResolveMap != NULL, "[reasset] bug! reasset_blocks_get_resolve_map block resolve map get failed.");

    return blockResolveMap->resolveMap;
}

_Bool reasset_blocks_is_base_id(s32 trkblkID, s32 id) {
    s32 numTrkblk = list_get_length(&trkblkList);
    if (reasset_trkblk_is_base_id(trkblkID) && id < numTrkblk) {
        TrkBlkEntry *trkblk = list_get(&trkblkList, trkblkID);
        
        return id >= 0 && id < trkblk->blocks.originalCount;
    }

    return FALSE;
}

// MARK: Hits

RECOMP_EXPORT void reasset_hits_set(ReAssetID trkblkID, ReAssetID blockID, ReAssetID id, ReAssetNamespace owner, const void *data) {
    reasset_assert_stage_set_call("reasset_hits_set");

    TrkBlkEntry *trkblk = get_or_create_trkblk(trkblkID);

    BlockEntry *block = get_or_create_block(trkblk, blockID);

    HitEntry *entry = get_or_create_hit(block, id);
    buffer_set(&entry->hit, data, sizeof(HitsLine));
    entry->owner = owner;
    entry->delete = FALSE;

    if (reasset_is_debug_logging_enabled()) {
        const char *trkblkNamespaceName;
        s32 trkblkIdentifier;
        reasset_id_lookup_name(trkblkID, &trkblkNamespaceName, &trkblkIdentifier);
        const char *blockNamespaceName;
        s32 blockIdentifier;
        reasset_id_lookup_name(blockID, &blockNamespaceName, &blockIdentifier);
        const char *namespaceName;
        s32 identifier;
        reasset_id_lookup_name(id, &namespaceName, &identifier);
        reasset_log_debug("[reasset] Hit set: %s:%d[%s:%d][%s:%d]\n", 
            trkblkNamespaceName, trkblkIdentifier,
            blockNamespaceName, blockIdentifier,
            namespaceName, identifier);
    }
}

RECOMP_EXPORT void reasset_hits_set_bulk(ReAssetID trkblkID, ReAssetID blockID, ReAssetNamespace owner, ReAssetID *idArray, const void **dataArray, s32 count) {
    reasset_assert_stage_set_call("reasset_hits_set");

    TrkBlkEntry *trkblk = get_or_create_trkblk(trkblkID);

    BlockEntry *block = get_or_create_block(trkblk, blockID);

    for (s32 i = 0; i < count; i++) {
        ReAssetID id = idArray[i];
        const void *data = dataArray[i];

        HitEntry *entry = get_or_create_hit(block, id);
        buffer_set(&entry->hit, data, sizeof(HitsLine));
        entry->owner = owner;
        entry->delete = FALSE;
    }

    const char *trkblkNamespaceName;
    s32 trkblkIdentifier;
    reasset_id_lookup_name(trkblkID, &trkblkNamespaceName, &trkblkIdentifier);
    const char *blockNamespaceName;
    s32 blockIdentifier;
    reasset_id_lookup_name(blockID, &blockNamespaceName, &blockIdentifier);
    reasset_log_debug("[reasset] Hit bulk set for %s:%d[%s:%d] (%d entries)\n", 
        trkblkNamespaceName, trkblkIdentifier,
        blockNamespaceName, blockIdentifier,
        count);
}

RECOMP_EXPORT void* reasset_hits_get(ReAssetID trkblkID, ReAssetID blockID, ReAssetID id) {
    reasset_assert_stage_get_call("reasset_hits_get");

    TrkBlkEntry *trkblk = get_trkblk(trkblkID);
    if (trkblk != NULL) {
        BlockEntry *block = get_block(trkblk, blockID);
        if (block != NULL) {
            HitEntry *entry = get_hit(block, id);
            if (entry != NULL) {
                if (reassetStage == REASSET_STAGE_RESOLVE) {
                    return bin_ptr_get(&entry->hitPtr, NULL);
                } else {
                    return buffer_get(&entry->hit, NULL);
                }
            }
        }
    }

    return NULL;
}

RECOMP_EXPORT void reasset_hits_delete(ReAssetID trkblkID, ReAssetID blockID, ReAssetID id) {
    reasset_assert_stage_delete_call("reasset_hits_delete");

    TrkBlkEntry *trkblk = get_trkblk(trkblkID);
    if (trkblk != NULL) {
        BlockEntry *block = get_block(trkblk, blockID);
        if (block != NULL) {
            HitEntry *entry = get_hit(block, id);
            if (entry != NULL) {
                entry->delete = TRUE;
            }
        }
    }
}

RECOMP_EXPORT ReAssetIterator reasset_hits_create_iterator(ReAssetID trkblkID, ReAssetID blockID) {
    reasset_assert_stage_iterator_call("reasset_hits_create_iterator");

    TrkBlkEntry *trkblk = get_trkblk(trkblkID);
    if (trkblk != NULL) {
        BlockEntry *block = get_block(trkblk, blockID);
        if (block != NULL) {
            return reasset_iterator_create(&block->hits.idList);
        }
    }

    return reasset_iterator_create(NULL);
}

RECOMP_EXPORT void reasset_hits_link(ReAssetID trkblkID, ReAssetID blockID, ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_hits_link");

    create_block_resolve_map(trkblkID);

    BlockResolveMap *blockResolveMap = recomputil_u32_memory_hashmap_get(trkblkResolveMap.blockResolveMaps, trkblkID);
    reasset_assert(blockResolveMap != NULL, "[reasset] bug! reasset_hits_link block resolve map get failed.");

    create_hit_resolve_map(blockResolveMap, blockID);

    ReAssetResolveMap hitResolveMap;
    reasset_assert(recomputil_u32_value_hashmap_get(blockResolveMap->hitResolveMaps, blockID, &hitResolveMap),
        "[reasset] bug! reasset_hits_link hit resolve map get failed.");

    reasset_resolve_map_link(hitResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_hits_get_resolve_map(ReAssetID trkblkID, ReAssetID blockID) {
    reasset_assert_stage_get_resolve_map_call("reasset_hits_get_resolve_map");

    create_block_resolve_map(trkblkID);
    
    BlockResolveMap *blockResolveMap = recomputil_u32_memory_hashmap_get(trkblkResolveMap.blockResolveMaps, trkblkID);
    reasset_assert(blockResolveMap != NULL, "[reasset] bug! reasset_hits_get_resolve_map block resolve map get failed.");

    create_hit_resolve_map(blockResolveMap, blockID);

    ReAssetResolveMap hitResolveMap;
    reasset_assert(recomputil_u32_value_hashmap_get(blockResolveMap->hitResolveMaps, blockID, &hitResolveMap),
        "[reasset] bug! reasset_hits_get_resolve_map hit resolve map get failed.");

    return hitResolveMap;
}
