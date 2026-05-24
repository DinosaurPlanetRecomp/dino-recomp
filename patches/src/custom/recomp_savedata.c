#include "patches.h"
#include "recomp_funcs.h"
#include "recompdata.h"
#include "recomp_savedata.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/files/reasset_bits.h"
#include "reasset/files/reasset_music_actions.h"
#include "reasset/files/reasset_maps.h"
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

typedef struct {
    s32 resolvedIdentifier;
    s32 mapID;
    ReAssetIDData *idData;
} MapObjIDLink;

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
        recomp_exit_with_error("[recompsave] recomp bug! extension_set_lookup failed.");
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

static void determine_maction_links(RecompFlashData *flash, ExtensionSet *extensions, IterableSet *mactionLinks) {
    ReAssetResolveMap mactionResolveMap = reasset_music_actions_get_resolve_map();
    for (s32 p = 0; p < 2; p++) {
        PlayerMusicActions *pmactions = &flash->base.asSave.map.musicActions[p];
        for (s32 i = 0; i < 4; i++) {
            s32 actionNo = pmactions->actionNums[i];
            if (actionNo <= 0) {
                continue;
            }

            // Save info about custom music actions that are in use
            ReAssetIDData *actionIDData;
            if (reasset_resolve_map_id_data_of(mactionResolveMap, actionNo - 1, &actionIDData) 
                    && actionIDData->namespace != REASSET_BASE_NAMESPACE) {
                extension_set_add(extensions, actionIDData->namespace);
                IDLink link = { .resolvedIdentifier = actionNo - 1, .idData = actionIDData };
                iterable_set_add(mactionLinks, actionNo - 1, &link);
            }
        }
    }
}

static void determine_map_object_links(RecompFlashData *flash, ExtensionSet *extensions, List *mapObjLinks) {
    ReAssetResolveMap mapResolveMap = reasset_maps_get_resolve_map();
    for (s32 i = 0; i < flash->base.asSave.file.numSavedObjects; i++) {
        SavedObject *savedObj = &flash->base.asSave.file.savedObjects[i];

        if (savedObj->mapID < 0) {
            // Can't link without a map ID. This saved object won't be restored by the game anyway without a map set.
            continue;
        }

        ReAssetID mapID;
        if (!reasset_resolve_map_id_of(mapResolveMap, savedObj->mapID, &mapID)) {
            // Not a map we know about, shouldn't be possible?
            recomp_eprintf("[recompsave] Unknown map ID %d in saved object %d (UID 0x%X). Recomp save link won't be created!\n", 
                savedObj->mapID, i, savedObj->uID);
            continue;
        }

        ReAssetResolveMap mapObjResolveMap = reasset_map_objects_get_resolve_map(mapID);

        // Save info about custom map objects that are in use
        ReAssetIDData *objIDData;
        if (reasset_resolve_map_id_data_of(mapObjResolveMap, savedObj->uID, &objIDData) 
                && objIDData->namespace != REASSET_BASE_NAMESPACE) {
            if (objIDData->identifier == -1) {
                // Cannot save map objects with auto IDs
                recomp_eprintf("[recompsave] WARN: Saved object %d (UID 0x%X, map %d) is defined as a ReAsset auto ID and thus cannot be a saved object!\n",
                    i, savedObj->uID, savedObj->mapID);
                continue;
            }

            extension_set_add(extensions, objIDData->namespace);
            MapObjIDLink link = { 
                .resolvedIdentifier = savedObj->uID, 
                .mapID = savedObj->mapID,
                .idData = objIDData
            };
            list_add_copy(mapObjLinks, &link);
        }
    }
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
    determine_maction_links(flash, &extensions, &mactionLinks); 
    
    List mapObjLinks;
    list_init(&mapObjLinks, sizeof(MapObjIDLink), 0);
    determine_map_object_links(flash, &extensions, &mapObjLinks);

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

    // Write map objects
    s32 numMapObjs = list_get_length(&mapObjLinks);
    header->numMapObjects = numMapObjs;
    for (s32 i = 0; i < numMapObjs; i++) {
        ptr = (u8*)mmAlign4((u32)ptr);

        MapObjIDLink *link = list_get(&mapObjLinks, i);
        u32 extension = extension_set_lookup(&extensions, link->idData->namespace);

        RecompSaveMapObjectLink *saveLink = (RecompSaveMapObjectLink*)ptr;
        saveLink->resolvedUID = link->resolvedIdentifier;
        saveLink->extension = (u8)extension;
        saveLink->mapID = (u16)link->mapID;
        saveLink->localID = link->idData->identifier;

        ptr += sizeof(RecompSaveMapObjectLink);
    }

    if (ptr > flashEndPtr) {
        recomp_eprintf("[recompsave] Overflow when writing recomp savedata! Savefile slot %d may be corrupt.", slotno);
    }

    // Clean up
    list_free(&mapObjLinks);
    iterable_set_free(&mactionLinks);
    extension_set_free(&extensions);
    list_free(&packedBitNamespaces);

    //recomp_printf("[recompsave] Wrote recomp savedata (slot %d).\n", slotno);
    recomp_savedata_on_saved(slotno);
}

static void remove_invalid_saved_objects(Savegame *savegame, U32List *invalidList) {
    s32 count = u32list_get_length(invalidList);
    for (s32 i = 0; i < count; i++) {
        u32 invalidIdx = u32list_get(invalidList, i);

        SavedObject *invalid = &savegame->file.savedObjects[invalidIdx];
        recomp_printf("[recompsave] Removing invalid saved object %d (UID 0x%X, map %d)\n", 
            invalidIdx, invalid->uID, invalid->mapID);

        for (s32 k = invalidIdx; k < (100 - 1); k++) {
            SavedObject *curr = &savegame->file.savedObjects[k];
            SavedObject *next = &savegame->file.savedObjects[k + 1];

            curr->uID = next->uID;
            curr->mapID = next->mapID;
            curr->_unk6 = next->_unk6;
            curr->x = next->x;
            curr->y = next->y;
            curr->z = next->z;
        }

        for (s32 j = 0; j < count; j++) {
            u32 idx = u32list_get(invalidList, j);
            if (idx > invalidIdx) {
                u32list_set(invalidList, j, idx - 1);
            }
        }

        savegame->file.numSavedObjects--;
    }
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
        //recomp_printf("[recompsave] Recomp savedata not found in slot %d (this is OK).\n", slotno);
        recomp_savedata_on_loaded(slotno);
        return;
    }
    if (header->version != 1) {
        recomp_eprintf("[recompsave] Recomp savedata (slot %d) has an invalid version: %d\n", slotno, header->version);
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
        savegame->main.bitString,
        savegame->file.bitString,
        savegame->map.bitString
    };
    u8 *bitstringSrc[3] = {
        recomp_alloc(sizeof(savegame->main.bitString)),
        recomp_alloc(sizeof(savegame->file.bitString)),
        recomp_alloc(sizeof(savegame->map.bitString))
    };
    bcopy(savegame->main.bitString, bitstringSrc[0], sizeof(savegame->main.bitString));
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
                recomp_eprintf("[recompsave] Dropping orphaned bitstring region %s:%d. The bitstring is full. Savefile slot: %d\n",
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
        PlayerMusicActions *pmactions = &savegame->map.musicActions[p];
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

    // Load map objects
    ReAssetResolveMap mapResolveMap = reasset_maps_get_resolve_map();
    IterableSet mapObjMaps;
    iterable_set_init(&mapObjMaps, sizeof(U32ValueHashmapHandle));
    for (s32 i = 0; i < header->numMapObjects && ptr < flashEndPtr; i++) {
        ptr = (u8*)mmAlign4((u32)ptr);

        RecompSaveMapObjectLink *link = (RecompSaveMapObjectLink*)ptr;
        ptr += sizeof(RecompSaveMapObjectLink);

        if (link->localID == -1) {
            // Cannot restore auto IDs
            continue;
        }

        ReAssetNamespace namespace = extNamespaces[link->extension];
        ReAssetID assetID = reasset_id(namespace, link->localID);

        // TODO: when we save map links, we'll need to go through that here
        ReAssetID mapID;
        if (!reasset_resolve_map_id_of(mapResolveMap, link->mapID, &mapID)) {
            // Not a map we know about
            continue;
        }

        ReAssetResolveMap mapObjResolveMap = reasset_map_objects_get_resolve_map(mapID);

        s32 newResolvedIdentifier = reasset_resolve_map_lookup(mapObjResolveMap, assetID);
        if (newResolvedIdentifier == -1) {
            // Not a map object we know about
            continue;
        }

        U32ValueHashmapHandle objMap;
        U32ValueHashmapHandle *objMapPtr;
        if (iterable_set_get(&mapObjMaps, link->mapID, (void**)&objMapPtr)) {
            objMap = *objMapPtr; // ughhhhh
        } else {
            objMap = recomputil_create_u32_value_hashmap();
            iterable_set_add(&mapObjMaps, link->mapID, &objMap);
        }
        recomputil_u32_value_hashmap_insert(objMap, link->resolvedUID, newResolvedIdentifier);
    }
    U32List invalidSavedObjs;
    u32list_init(&invalidSavedObjs, 0);
    for (s32 i = 0; i < savegame->file.numSavedObjects; i++) {
        SavedObject *savedObj = &savegame->file.savedObjects[i];

        ReAssetID mapID;
        if (!reasset_resolve_map_id_of(mapResolveMap, savedObj->mapID, &mapID)) {
            // Saved object references a map we don't have anymore
            u32list_add(&invalidSavedObjs, i);
            continue;
        }

        if (reasset_map_objects_is_base_uid(mapID, savedObj->uID)) {
            continue;
        }

        U32ValueHashmapHandle *objMapPtr;
        if (!iterable_set_get(&mapObjMaps, savedObj->mapID, (void**)&objMapPtr)) {
            // No hashmap for new identifiers exists, so we can't restore this object
            u32list_add(&invalidSavedObjs, i);
            continue;
        }
        
        // Remap to the new identifier of the original asset, if a mod is still providing it
        s32 newResolvedIdentifier;
        if (recomputil_u32_value_hashmap_get(*objMapPtr, savedObj->uID, (u32*)&newResolvedIdentifier)) {
            recomp_printf("[recompsave] Remapping saved object %d UID 0x%X -> 0x%X\n", i, savedObj->uID, newResolvedIdentifier);
            savedObj->uID = newResolvedIdentifier;
        } else {
            u32list_add(&invalidSavedObjs, i);
        }
    }
    remove_invalid_saved_objects(savegame, &invalidSavedObjs);

    if (ptr > flashEndPtr) {
        recomp_eprintf("[recompsave] Overflow when reading recomp savedata! Savefile slot %d may be corrupt.", slotno);
    }

    // Clean up
    u32list_free(&invalidSavedObjs);
    {
        List *mapObjMapsList = iterable_set_get_list(&mapObjMaps);
        s32 numMapObjMaps = list_get_length(mapObjMapsList);
        for (s32 i = 0; i < numMapObjMaps; i++) {
            recomputil_destroy_u32_value_hashmap(*(U32ValueHashmapHandle*)list_get(mapObjMapsList, i));
        }
        iterable_set_free(&mapObjMaps);
    }
    recomputil_destroy_u32_value_hashmap(mactionMap);
    for (s32 i = 0; i < (s32)ARRAYCOUNT(bitstringSrc); i++) {
        recomp_free(bitstringSrc[i]);
    }
    if (extNamespaces != NULL) {
        recomp_free(extNamespaces);
    }

    //recomp_printf("[recompsave] Loaded recomp savedata from slot %d.\n", slotno);
    recomp_savedata_on_loaded(slotno);
}

static void validate_extension_name(const char *name) {
    // Must match reasset namespace name requirements
    s32 nameLen = strlen(name);
    if (nameLen <= 0 || nameLen > 16) {
        recomp_exit_with_error(recomp_sprintf_helper(
            "[recompsave] Invalid recomp savedata extension name: \"%s\". Extension names must be at least one character long and less than or equal to 16 characters long.",
            name));
    }
}

RECOMP_EXPORT void recomp_savedata_set_custom_data(const char *extensionName, void *data, u32 sizeBytes) {
    validate_extension_name(extensionName);
    if (!bInitialized) {
        recomp_exit_with_error("[recompsave] Cannot call recomp_savedata_set_custom_data before a savegame has been loaded!");
    }

    custom_data_list_set(reasset_namespace(extensionName), data, sizeBytes);
}

RECOMP_EXPORT int recomp_savedata_get_custom_data(const char *extensionName, void **outData, u32 *outSizeBytes) {
    validate_extension_name(extensionName);
    if (!bInitialized) {
        recomp_exit_with_error("[recompsave] Cannot call recomp_savedata_get_custom_data before a savegame has been loaded!");
    }

    return custom_data_list_get(reasset_namespace(extensionName), outData, outSizeBytes) ? 1 : 0;
}
