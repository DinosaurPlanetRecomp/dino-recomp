#include "reasset_bits.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/list.h"

#include "PR/ultratypes.h"
#include "sys/fs.h"
#include "sys/main.h"
#include "sys/memory.h"
#include "macros.h"

typedef struct {
    ReAssetID id;
    u8 length;
    u8 save;
    s16 task;
} BitEntry;

typedef struct {
    ReAssetNamespace namespace;
    List list; // list[BitEntry]
    U32ValueHashmapHandle map; // ReAssetID -> list index
} BitList;

static List packedNamespaces; // list[PackedBitRanges]
static U32ValueHashmapHandle packedNamespacesMap; // ReAssetNamespace -> list index

static List bitsNamespaceList; // list[BitList]
static U32ValueHashmapHandle bitsNamespaceMap; // ReAssetNamespace -> list index
static ReAssetResolveMap bitsResolveMap;

// current bit offset for each string. used for packing
static s32 bitstringOffsets[4];
static s32 bitstringMaxLengths[4];

static s32 orphanBitstringOffsets[4];
static List orphanPackedNamespaces; // list[PackedBitRanges]
static U32ValueHashmapHandle orphanPackedNamespacesMap; // ReAssetNamespace -> list index

static void bits_namespace_list_element_free(void *element) {
    BitList *entry = element;
    list_free(&entry->list);
    recomputil_destroy_u32_value_hashmap(entry->map);
}

static BitEntry* get_or_create_bit(ReAssetID id) {
    ReAssetIDData *idData = reasset_id_lookup_data(id);
    reasset_assert(idData->namespace != REASSET_BASE_NAMESPACE, 
        "[reasset] Creating/modifying base bits is not supported.");

    BitList *list;
    u32 nmListIdx;
    if (!recomputil_u32_value_hashmap_get(bitsNamespaceMap, idData->namespace, &nmListIdx)) {
        // Create bit list for namespace
        nmListIdx = list_get_length(&bitsNamespaceList);
        list = list_add(&bitsNamespaceList);
        list->namespace = idData->namespace;
        list_init(&list->list, sizeof(BitEntry), 0);
        list->map = recomputil_create_u32_value_hashmap();

        recomputil_u32_value_hashmap_insert(bitsNamespaceMap, idData->namespace, nmListIdx);
    } else {
        list = list_get(&bitsNamespaceList, nmListIdx);
    }

    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(list->map, id, &listIdx)) {
        // Create bit entry
        listIdx = list_get_length(&list->list);
        BitEntry *entry = list_add(&list->list);
        entry->id = id;

        recomputil_u32_value_hashmap_insert(list->map, id, listIdx);
    }

    return list_get(&list->list, listIdx);
}

void reasset_bits_init(void) {
    list_init(&packedNamespaces, sizeof(PackedBitRanges), 0);
    packedNamespacesMap = recomputil_create_u32_value_hashmap();

    list_init(&orphanPackedNamespaces, sizeof(PackedBitRanges), 0);
    orphanPackedNamespacesMap = recomputil_create_u32_value_hashmap();

    list_init(&bitsNamespaceList, sizeof(BitList), 0);
    list_set_element_free_callback(&bitsNamespaceList, bits_namespace_list_element_free);
    bitsNamespaceMap = recomputil_create_u32_value_hashmap();
    bitsResolveMap = reasset_resolve_map_create("Bits");

    bitstringMaxLengths[0] = 128 * 8;
    bitstringMaxLengths[1] = 512 * 8;
    bitstringMaxLengths[2] = 256 * 8;
    bitstringMaxLengths[3] = 256 * 8;
}

void reasset_bits_repack(void) {
    // Load original bittable
    s32 bittableOriginalCount = reasset_fst_get_file_size(BITTABLE_BIN) / sizeof(BitTableEntry);
    BitTableEntry *bittableOriginal = reasset_fst_alloc_load_file(BITTABLE_BIN, NULL);

    // Calculate end of each bitstring to determine where we can add new bits
    s32 originalBitEnds[4] = {0};
    for (s32 i = 0; i < bittableOriginalCount; i++) {
        BitTableEntry *bit = &bittableOriginal[i];
        s32 saveType = (bit->field_0x2 >> 6) & 0x3;
        s32 length = (bit->field_0x2 & 0x1f) + 1;
        originalBitEnds[saveType] = MAX(originalBitEnds[saveType], bit->start + length);
    }
    s32 originalByteEnds[4];
    for (s32 i = 0; i < 4; i++) {
        originalByteEnds[i] = mmAlign8(originalBitEnds[i]) / 8;
    }

    // Calculate new bittable length
    s32 newBittableCount = bittableOriginalCount;
    s32 bitsNamespaceCount = list_get_length(&bitsNamespaceList);
    for (s32 i = 0; i < bitsNamespaceCount; i++) {
        BitList *list = list_get(&bitsNamespaceList, i);
        newBittableCount += list_get_length(&list->list);
    }

    // Alloc new file
    u32 bittableBinSize = newBittableCount * sizeof(BitTableEntry);
    BitTableEntry *bittableBin = recomp_alloc(bittableBinSize);

    // Copy over base data
    bcopy(bittableOriginal, bittableBin, bittableOriginalCount * sizeof(BitTableEntry));

    // Pack in new bits
    s32 bittableIndex = bittableOriginalCount;
    for (s32 i = 0; i < 4; i++) {
        // Always start namespace on a byte boundary for simplicity later
        bitstringOffsets[i] = mmAlign8(originalBitEnds[i]);
    }
    for (s32 i = 0; i < bitsNamespaceCount; i++) {
        s32 startOffsets[4];
        for (s32 i = 0; i < 4; i++) {
            startOffsets[i] = bitstringOffsets[i] / 8;
        }

        // Add new bits
        BitList *list = list_get(&bitsNamespaceList, i);
        s32 listCount = list_get_length(&list->list);
        for (s32 j = 0; j < listCount; j++) {
            BitEntry *entry = list_get(&list->list, j);

            s32 identifier;
            const char *namespaceName;
            reasset_id_lookup_name(entry->id, &namespaceName, &identifier);
            reasset_log("[reasset] New bit: %s:%d\n", namespaceName, identifier);

            reasset_resolve_map_resolve_id(bitsResolveMap, entry->id, list->namespace, bittableIndex);

            BitTableEntry *binEntry = &bittableBin[bittableIndex];
            binEntry->start = bitstringOffsets[entry->save];
            binEntry->field_0x2 = ((entry->save & 0x3) << 6) 
                | ((entry->task >= 0 ? 1 : 0) << 5) 
                | ((entry->length - 1) & 0x1F);
            binEntry->task = entry->task >= 0 ? entry->task : 0;
            
            bitstringOffsets[entry->save] += entry->length;

            reasset_assert((bitstringOffsets[entry->save] - 1) < bitstringMaxLengths[entry->save], 
                "[reasset:reasset_bits_repack] Overflowed bitstring %d while packing, too many bits are registered!",
                entry->save);

            bittableIndex += 1;
        }

        for (s32 i = 0; i < 4; i++) {
            bitstringOffsets[i] = mmAlign8(bitstringOffsets[i]);
        }
        s32 endOffsets[4];
        for (s32 i = 0; i < 4; i++) {
            endOffsets[i] = bitstringOffsets[i] / 8;
        }

        // Record where the namespace was packed for later
        s32 listIdx = list_get_length(&packedNamespaces);
        PackedBitRanges *packedRanges = list_add(&packedNamespaces);
        packedRanges->namespace = list->namespace;
        for (s32 i = 1; i < 4; i++) {
            packedRanges->ranges[i - 1].start = startOffsets[i];
            packedRanges->ranges[i - 1].end = endOffsets[i];
        }
        recomputil_u32_value_hashmap_insert(packedNamespacesMap, packedRanges->namespace, listIdx);
    }

    reasset_assert(bittableIndex <= newBittableCount, 
        "[reasset] Overflow writing BITTABLE.bin. %d > %d", bittableIndex, newBittableCount);

    // Finalize resolve map
    reasset_resolve_map_finalize(bitsResolveMap);

    // Set new file
    reasset_fst_set_internal(BITTABLE_BIN, bittableBin, bittableBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log("[reasset] Rebuilt BITTABLE.bin (count: %d).\n", newBittableCount);

    // Clean up
    recomp_free(bittableOriginal);
}

void reasset_bits_cleanup(void) {
    list_free(&bitsNamespaceList);
    recomputil_destroy_u32_value_hashmap(bitsNamespaceMap);
}

RECOMP_EXPORT void reasset_bits_add_task(ReAssetID id, s32 length, BitSaveType saveType, s32 task) {
    reasset_assert_stage_set_call("reasset_bits_add_task");

    reasset_assert(length > 0 && length <= 32, 
        "[reasset:reasset_bits_add] Bit length must be between 1 and 32.");
    reasset_assert(saveType >= 0 && saveType < 4, 
        "[reasset:reasset_bits_add] Bit save type must be between 0 and 3.");
    
    BitEntry *entry = get_or_create_bit(id);
    entry->length = length;
    entry->save = saveType;
    entry->task = task;
}

RECOMP_EXPORT void reasset_bits_add(ReAssetID id, s32 length, BitSaveType saveType) {
    reasset_assert_stage_set_call("reasset_bits_add");

    reasset_bits_add_task(id, length, saveType, -1);
}

RECOMP_EXPORT ReAssetResolveMap reasset_bits_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_bits_get_resolve_map");

    return bitsResolveMap;
}

List* reasset_bits_get_packed_namespaces(void) {
    return &packedNamespaces;
}

_Bool reasset_bits_get_packed_namespace(ReAssetNamespace namespace, PackedBitRanges **outRanges) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(packedNamespacesMap, namespace, &listIdx)) {
        if (outRanges != NULL) {
            *outRanges = list_get(&packedNamespaces, listIdx);
        }
        return TRUE;
    } else if (recomputil_u32_value_hashmap_get(orphanPackedNamespacesMap, namespace, &listIdx)) {
        if (outRanges != NULL) {
            *outRanges = list_get(&orphanPackedNamespaces, listIdx);
        }
        return TRUE;
    } else {
        if (outRanges != NULL) {
            *outRanges = NULL;
        }
        return FALSE;
    }
}

void reasset_bits_orphan_init(void) {
    // Reset for new savedata
    list_clear(&orphanPackedNamespaces);
    recomputil_destroy_u32_value_hashmap(orphanPackedNamespacesMap);
    orphanPackedNamespacesMap = recomputil_create_u32_value_hashmap();

    for (s32 i = 0; i < (s32)ARRAYCOUNT(bitstringOffsets); i++) {
        orphanBitstringOffsets[i] = bitstringOffsets[i];
    }
}

List* reasset_bits_orphan_get_packed_namespaces(void) {
    return &orphanPackedNamespaces;
}

PackedBitRanges* reasset_bits_orphan_add(ReAssetNamespace namespace, BitSaveType saveType, s32 length) {
    reasset_assert(saveType > 0, "[reasset] bug! reasset_bits_orphan_add saveType cannot be 'never'.");
    
    if (orphanBitstringOffsets[saveType] + (length * 8) - 1 >= bitstringMaxLengths[saveType]) {
        // No room
        return NULL;
    }

    // Get namespace ranges
    u32 listIdx;
    PackedBitRanges *packedRanges;
    if (recomputil_u32_value_hashmap_get(orphanPackedNamespacesMap, namespace, &listIdx)) {
        packedRanges = list_get(&orphanPackedNamespaces, listIdx);
    } else {
        listIdx = list_get_length(&orphanPackedNamespaces);
        packedRanges = list_add(&orphanPackedNamespaces);
        packedRanges->namespace = namespace;

        recomputil_u32_value_hashmap_insert(orphanPackedNamespacesMap, namespace, listIdx);
    }

    // Append
    s32 byteOffset = orphanBitstringOffsets[saveType] / 8;
    packedRanges->ranges[saveType - 1].start = byteOffset;
    packedRanges->ranges[saveType - 1].end = byteOffset + length;

    orphanBitstringOffsets[saveType] += length * 8;

    return packedRanges;
}
