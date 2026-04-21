#include "patches.h"
#include "recomp_funcs.h"
#include "recompdata.h"
#include "recomp_savedata.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/files/reasset_bits.h"
#include "reasset/files/reasset_music_actions.h"
#include "reasset/list.h"
#include "reasset/iterable_set.h"

#include "PR/ultratypes.h"
#include "PR/os.h"
#include "libc/string.h"
#include "dlls/engine/29_gplay.h"
#include "sys/memory.h"
#include "macros.h"

RECOMP_DECLARE_EVENT(recomp_savedata_on_load(s32 slotno));
RECOMP_DECLARE_EVENT(recomp_savedata_on_loaded(s32 slotno));
RECOMP_DECLARE_EVENT(recomp_savedata_on_save(s32 slotno));
RECOMP_DECLARE_EVENT(recomp_savedata_on_saved(s32 slotno));

typedef struct {
    U32List list;
    U32ValueHashmapHandle map; // ReAssetNamespace -> list idx
} ExtensionSet;

typedef struct {
    ReAssetNamespace namespace;
    void *ptr;
    u32 sizeBytes;
} CustomSaveData;

// Links a resolved identifier to its namespace:localID pair
typedef struct {
    s32 resolvedIdentifier;
    ReAssetIDData *idData;
} IDLink;

static U32ValueHashmapHandle customDataMap; // ReAssetNamespace -> list idx
static List customDataList; // list[CustomSaveData]
static _Bool bInitialized = FALSE;

static void extension_set_init(ExtensionSet *set) {
    bzero(set, sizeof(ExtensionSet));
    u32list_init(&set->list, 0);
    set->map = recomputil_create_u32_value_hashmap();
}

static void extension_set_free(ExtensionSet *set) {
    u32list_free(&set->list);
    recomputil_destroy_u32_value_hashmap(set->map);
}

static void extension_set_add(ExtensionSet *set, ReAssetNamespace namespace) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(set->map, namespace, &listIdx)) {
        listIdx = u32list_add(&set->list, namespace);

        recomputil_u32_value_hashmap_insert(set->map, namespace, listIdx);
    }
}

static u32 extension_set_lookup(ExtensionSet *set, ReAssetNamespace namespace) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(set->map, namespace, &listIdx)) {
        recomp_exit_with_error("recomp bug! extension_set_lookup failed.");
    }

    return listIdx;
}

static void custom_data_list_element_free_callback(void *element) {
    CustomSaveData *customData = element;
    if (customData->ptr != NULL) {
        recomp_free(customData->ptr);
        customData->ptr = NULL;
        customData->sizeBytes = 0;
        customData->namespace = REASSET_INVALID_NAMESPACE;
    }
}

static void custom_data_list_clear(void) {
    list_clear(&customDataList);
    recomputil_destroy_u32_value_hashmap(customDataMap);
    customDataMap = recomputil_create_u32_value_hashmap();
}

static void custom_data_list_set(ReAssetNamespace namespace, void *data, u32 sizeBytes) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(customDataMap, namespace, &listIdx)) {
        listIdx = list_get_length(&customDataList);
        list_add(&customDataList);

        recomputil_u32_value_hashmap_insert(customDataMap, namespace, listIdx);
    }

    CustomSaveData *customData = list_get(&customDataList, listIdx);
    if (customData->ptr != NULL && customData->sizeBytes != sizeBytes) {
        recomp_free(customData->ptr);
        customData->ptr = NULL;
    }
    if (customData->ptr == NULL && sizeBytes != 0) {
        customData->ptr = recomp_alloc(sizeBytes);
    }

    bcopy(data, customData->ptr, sizeBytes);
    customData->sizeBytes = sizeBytes;

    customData->namespace = namespace;
}

static _Bool custom_data_list_get(ReAssetNamespace namespace, void **outData, u32 *outSizeBytes) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(customDataMap, namespace, &listIdx)) {
        // Not found
        if (outData != NULL) {
            *outData = NULL;
        }
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        
        return NULL;
    }

    CustomSaveData *customData = list_get(&customDataList, listIdx);
    if (outData != NULL) {
        *outData = customData->ptr;
    }
    if (outSizeBytes != NULL) {
        *outSizeBytes = customData->sizeBytes;
    }

    return TRUE;
}

static void recomp_savedata_init(void) {
    if (bInitialized) return;

    customDataMap = recomputil_create_u32_value_hashmap();
    list_init(&customDataList, sizeof(CustomSaveData), 0);
    list_set_element_free_callback(&customDataList, custom_data_list_element_free_callback);

    bInitialized = TRUE;
}

void recomp_savedata_save(RecompFlashData *flash, s32 slotno) {
    if (!bInitialized) recomp_savedata_init();

    // Give mods a chance to set custom data
    recomp_savedata_on_save(slotno);

    // Setup
    u8 *ptr = (u8*)&flash->recomp + sizeof(RecompSaveDataHeader);
    u8 *flashEndPtr = (u8*)flash + 0x4000;

    bzero(&flash->recomp, (u32)(flashEndPtr - (u8*)&flash->recomp));

    RecompSaveDataHeader *header = &flash->recomp;
    header->magic[0] = 'R';
    header->magic[1] = 'C';
    header->version = 1;

    // Determine extensions
    ExtensionSet extensions;
    extension_set_init(&extensions);

    List packedBitNamespaces;
    list_init(&packedBitNamespaces, sizeof(PackedBitRanges), 
        list_get_length(reasset_bits_get_packed_namespaces()) + list_get_length(reasset_bits_orphan_get_packed_namespaces()));
    list_add_range(&packedBitNamespaces, reasset_bits_get_packed_namespaces());
    list_add_range(&packedBitNamespaces, reasset_bits_orphan_get_packed_namespaces());

    s32 numPackedBitNamespaces = list_get_length(&packedBitNamespaces);
    for (s32 i = 0; i < numPackedBitNamespaces; i++) {
        PackedBitRanges *ranges = list_get(&packedBitNamespaces, i);
        extension_set_add(&extensions, ranges->namespace);
    }

    s32 numCustomSaveDataEntries = list_get_length(&customDataList);
    for (s32 i = 0; i < numCustomSaveDataEntries; i++) {
        CustomSaveData *customData = list_get(&customDataList, i);
        extension_set_add(&extensions, customData->namespace);
    }

    IterableSet mactionLinks;
    iterable_set_init(&mactionLinks, sizeof(IDLink));
    ReAssetResolveMap mactionResolveMap = reasset_music_actions_get_resolve_map();
    for (s32 p = 0; p < 2; p++) {
        PlayerMusicAction *pmactions = &flash->base.asSave.map.unk179C[p];
        for (s32 i = 0; i < 4; i++) {
            s32 actionNo = pmactions->actionNums[i];
            if (actionNo <= 0) {
                continue;
            }

            // Save info about custom music actions that are in use
            ReAssetIDData *actionIDData;
            if (reasset_resolve_map_id_data_of(mactionResolveMap, actionNo - 1, &actionIDData) 
                    && actionIDData->namespace != REASSET_BASE_NAMESPACE) {
                extension_set_add(&extensions, actionIDData->namespace);
                IDLink link = { .resolvedIdentifier = actionNo - 1, .idData = actionIDData };
                iterable_set_add(&mactionLinks, actionNo - 1, &link);
            }
        }
    }

    // Write extensions
    header->numExtensions = u32list_get_length(&extensions.list);
    for (s32 i = 0; i < header->numExtensions && ptr < flashEndPtr; i++) {
        ptr = (u8*)mmAlign2((u32)ptr);

        RecompSaveExtensionHeader *extHeader = (RecompSaveExtensionHeader*)ptr;
        ptr += sizeof(RecompSaveExtensionHeader);

        extHeader->reserved = 0;

        // Write namespace name
        ReAssetNamespace namespace = u32list_get(&extensions.list, i);
        const char *namespaceName;
        reasset_namespace_lookup_name(namespace, &namespaceName);

        extHeader->namespaceNameLength = MIN(16, strlen(namespaceName));
        bcopy(namespaceName, ptr, extHeader->namespaceNameLength);
        ptr += extHeader->namespaceNameLength;

        // Write custom data
        void *customDataPtr;
        u32 customDataSizeBytes;
        if (custom_data_list_get(namespace, &customDataPtr, &customDataSizeBytes)) {
            bcopy(customDataPtr, ptr, customDataSizeBytes);
            extHeader->sizeBytes = customDataSizeBytes;
        } else {
            extHeader->sizeBytes = 0;
        }

        ptr += extHeader->sizeBytes;
    }

    // Write bit ranges
    for (s32 i = 0; i < numPackedBitNamespaces && ptr < flashEndPtr; i++) {
        PackedBitRanges *ranges = list_get(&packedBitNamespaces, i);
        u32 extension = extension_set_lookup(&extensions, ranges->namespace);

        for (s32 k = 0; k < 3 && ptr < flashEndPtr; k++) {
            PackedBitRange *range = &ranges->ranges[k];
            if ((range->start == 0 && range->end == 0) || (range->start == range->end)) {
                continue;
            }

            ptr = (u8*)mmAlign2((u32)ptr);

            RecompSaveBitstring *region = (RecompSaveBitstring*)ptr;
            ptr += sizeof(RecompSaveBitstring);

            region->extension = extension;
            region->bitstring = k + 1;
            region->startOffset = range->start;
            region->endOffset = range->end;

            header->numBitstrings += 1;
        }
    }

    // Write music actions
    List *mactionLinkList = iterable_set_get_list(&mactionLinks);
    s32 numMActions = list_get_length(mactionLinkList);
    header->numMusicActions = numMActions;
    for (s32 i = 0; i < numMActions; i++) {
        ptr = (u8*)mmAlign4((u32)ptr);

        IDLink *link = list_get(mactionLinkList, i);
        u32 extension = extension_set_lookup(&extensions, link->idData->namespace);
        
        RecompSaveMusicActionLink *saveLink = (RecompSaveMusicActionLink*)ptr;
        saveLink->resolvedIdentifier = link->resolvedIdentifier;
        saveLink->extension = (u8)extension;
        saveLink->localID = link->idData->identifier;

        ptr += sizeof(RecompSaveMusicActionLink);
    }

    if (ptr > flashEndPtr) {
        recomp_eprintf("Overflow when writing recomp savedata! Savefile slot %d may be corrupt.", slotno);
    }

    // Clean up
    iterable_set_free(&mactionLinks);
    extension_set_free(&extensions);
    list_free(&packedBitNamespaces);

    //recomp_printf("Wrote recomp savedata (slot %d).\n", slotno);
    recomp_savedata_on_saved(slotno);
}

void recomp_savedata_load(RecompFlashData *flash, s32 slotno) {
    if (!bInitialized) recomp_savedata_init();

    recomp_savedata_on_load(slotno);
    
    // Reset custom data
    custom_data_list_clear();
    // Reset savegame specific reasset data
    reasset_bits_orphan_init();

    Savegame *savegame = &flash->base.asSave;

    RecompSaveDataHeader *header = &flash->recomp;
    if (header->magic[0] != 'R' || header->magic[1] != 'C') {
        // Missing or invalid recomp savedata
        //recomp_printf("Recomp savedata not found in slot %d (this is OK).\n", slotno);
        recomp_savedata_on_loaded(slotno);
        return;
    }
    if (header->version != 1) {
        recomp_eprintf("Recomp savedata (slot %d) has an invalid version: %d\n", slotno, header->version);
        recomp_savedata_on_loaded(slotno);
        return;
    }

    u8 *ptr = (u8*)&flash->recomp + sizeof(RecompSaveDataHeader);
    u8 *flashEndPtr = (u8*)flash + 0x4000;

    // Load extensions
    ReAssetNamespace *extNamespaces = header->numExtensions == 0 
        ? NULL 
        : recomp_alloc(sizeof(ReAssetNamespace) * header->numExtensions);

    for (s32 i = 0; i < header->numExtensions && ptr < flashEndPtr; i++) {
        ptr = (u8*)mmAlign2((u32)ptr);

        RecompSaveExtensionHeader *extHeader = (RecompSaveExtensionHeader*)ptr;
        ptr += sizeof(RecompSaveExtensionHeader);
        
        // Load namespace name
        char namespaceName[17];
        bzero(namespaceName, sizeof(namespaceName));
        bcopy(ptr, namespaceName, MIN(extHeader->namespaceNameLength, 16));
        namespaceName[16] = '\0';

        ReAssetNamespace namespace = reasset_namespace(namespaceName);
        extNamespaces[i] = namespace;

        ptr += extHeader->namespaceNameLength;

        // Load custom savedata
        if (extHeader->sizeBytes > 0) {
            custom_data_list_set(namespace, ptr, extHeader->sizeBytes);
        }

        ptr += extHeader->sizeBytes;
    }

    // Load bitstring regions
    u8 *bitstringDst[3] = {
        savegame->chkpnt.bitString,
        savegame->file.bitString,
        savegame->map.bitString
    };
    u8 *bitstringSrc[3] = {
        recomp_alloc(sizeof(savegame->chkpnt.bitString)),
        recomp_alloc(sizeof(savegame->file.bitString)),
        recomp_alloc(sizeof(savegame->map.bitString))
    };
    bcopy(savegame->chkpnt.bitString, bitstringSrc[0], sizeof(savegame->chkpnt.bitString));
    bcopy(savegame->file.bitString, bitstringSrc[1], sizeof(savegame->file.bitString));
    bcopy(savegame->map.bitString, bitstringSrc[2], sizeof(savegame->map.bitString));

    for (s32 i = 0; i < header->numBitstrings && ptr < flashEndPtr; i++) {
        ptr = (u8*)mmAlign2((u32)ptr);

        RecompSaveBitstring *region = (RecompSaveBitstring*)ptr;
        ptr += sizeof(RecompSaveBitstring);
        
        if (region->bitstring <= 0 || region->bitstring >= 4 || region->extension >= header->numExtensions) {
            // Invalid region
            continue;
        }

        ReAssetNamespace namespace = extNamespaces[region->extension];
        s32 regionLength = region->endOffset - region->startOffset;

        // Get or create reasset entry for this bitstring
        PackedBitRanges *newRegions;
        if (!reasset_bits_get_packed_namespace(namespace, &newRegions)) {
            // Region exists in the savegame but no mod registered an entry for its namespace.
            // Register an orphaned range so we don't lose the data.
            newRegions = reasset_bits_orphan_add(namespace, region->bitstring, regionLength);
        
            if (newRegions == NULL) {
                // No room left in the bitstring. Drop the orphaned data to prioritize the current mod list
                const char *namespaceName;
                reasset_namespace_lookup_name(namespace, &namespaceName);
                recomp_eprintf("Dropping orphaned bitstring region %s:%d. The bitstring is full. Savefile slot: %d\n",
                    namespaceName, region->bitstring, slotno);
                continue;
            }
        }
        PackedBitRange *newRegion = &newRegions->ranges[region->bitstring - 1];
        s32 newRegionLength = newRegion->end - newRegion->start;

        // Copy existing data into new slot
        u8 *src = bitstringSrc[region->bitstring - 1];
        u8 *dst = bitstringDst[region->bitstring - 1];

        bcopy(src, dst, MIN(regionLength, newRegionLength));
        if (newRegionLength > regionLength) {
            bzero(dst + regionLength, newRegionLength - regionLength);
        }
    }

    // Load music action IDs
    ReAssetResolveMap mactionResolveMap = reasset_music_actions_get_resolve_map();
    U32ValueHashmapHandle mactionMap = recomputil_create_u32_value_hashmap();
    for (s32 i = 0; i < header->numMusicActions && ptr < flashEndPtr; i++) {
        ptr = (u8*)mmAlign4((u32)ptr);

        RecompSaveMusicActionLink *link = (RecompSaveMusicActionLink*)ptr;
        ptr += sizeof(RecompSaveMusicActionLink);

        ReAssetNamespace namespace = extNamespaces[link->extension];
        ReAssetID assetID = reasset_id(namespace, link->localID);
        
        s32 newResolvedIdentifier = reasset_resolve_map_lookup(mactionResolveMap, assetID);
        recomputil_u32_value_hashmap_insert(mactionMap, link->resolvedIdentifier, newResolvedIdentifier);
    }
    for (s32 p = 0; p < 2; p++) {
        PlayerMusicAction *pmactions = &savegame->map.unk179C[p];
        for (s32 i = 0; i < 4; i++) {
            s32 actionNo = pmactions->actionNums[i];
            if (actionNo <= 0 || reasset_music_actions_is_base_id(actionNo - 1)) {
                continue;
            }

            // Remap to the new identifier of the original asset, if a mod is still providing it
            s32 newResolvedIdentifier;
            if (recomputil_u32_value_hashmap_get(mactionMap, actionNo - 1, (u32*)&newResolvedIdentifier)) {
                pmactions->actionNums[i] = newResolvedIdentifier;
            } else {
                pmactions->actionNums[i] = -1;
            }
        }
    }

    if (ptr > flashEndPtr) {
        recomp_eprintf("Overflow when reading recomp savedata! Savefile slot %d may be corrupt.", slotno);
    }

    // Clean up
    recomputil_destroy_u32_value_hashmap(mactionMap);
    for (s32 i = 0; i < (s32)ARRAYCOUNT(bitstringSrc); i++) {
        recomp_free(bitstringSrc[i]);
    }
    if (extNamespaces != NULL) {
        recomp_free(extNamespaces);
    }

    //recomp_printf("Loaded recomp savedata from slot %d.\n", slotno);
    recomp_savedata_on_loaded(slotno);
}

static void validate_extension_name(const char *name) {
    // Must match reasset namespace name requirements
    s32 nameLen = strlen(name);
    if (nameLen <= 0 || nameLen > 16) {
        recomp_exit_with_error(recomp_sprintf_helper(
            "Invalid recomp savedata extension name: \"%s\". Extension names must be at least one character long and less than or equal to 16 characters long.",
            name));
    }
}

RECOMP_EXPORT void recomp_savedata_set_custom_data(const char *extensionName, void *data, u32 sizeBytes) {
    validate_extension_name(extensionName);
    if (!bInitialized) {
        recomp_exit_with_error("Cannot call recomp_savedata_set_custom_data before a savegame has been loaded!");
    }

    custom_data_list_set(reasset_namespace(extensionName), data, sizeBytes);
}

RECOMP_EXPORT int recomp_savedata_get_custom_data(const char *extensionName, void **outData, u32 *outSizeBytes) {
    validate_extension_name(extensionName);
    if (!bInitialized) {
        recomp_exit_with_error("Cannot call recomp_savedata_get_custom_data before a savegame has been loaded!");
    }

    return custom_data_list_get(reasset_namespace(extensionName), outData, outSizeBytes) ? 1 : 0;
}
