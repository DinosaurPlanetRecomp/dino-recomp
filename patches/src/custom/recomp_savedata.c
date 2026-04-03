#include "patches.h"
#include "recomp_funcs.h"
#include "recompdata.h"
#include "recomp_savedata.h"
#include "reasset/reasset_namespace.h"
#include "reasset/files/reasset_bits.h"
#include "reasset/list.h"

#include "PR/ultratypes.h"
#include "libc/string.h"
#include "dlls/engine/29_gplay.h"
#include "sys/memory.h"
#include "macros.h"

typedef struct {
    U32List list;
    U32ValueHashmapHandle map; // ReAssetNamespace -> list idx
} ExtensionSet;

static void extension_set_init(ExtensionSet *set) {
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

void recomp_savedata_save(RecompFlashData *flash) {
    u8 *ptr = (u8*)&flash->recomp + sizeof(RecompSaveDataHeader);
    u8 *flashEndPtr = (u8*)flash + 0x4000;

    bzero(&flash->recomp, (u32)(flashEndPtr - (u8*)&flash->recomp));

    RecompSaveDataHeader *header = &flash->recomp;
    header->magic[0] = 'R';
    header->magic[1] = 'C';
    header->version = 1;

    // Determine extensions
    ExtensionSet extensions = {0};
    extension_set_init(&extensions);

    List packedBitNamespaces = {0};
    list_init(&packedBitNamespaces, sizeof(PackedBitRanges), 
        list_get_length(reasset_bits_get_packed_namespaces()) + list_get_length(reasset_bits_orphan_get_packed_namespaces()));
    list_add_range(&packedBitNamespaces, reasset_bits_get_packed_namespaces());
    list_add_range(&packedBitNamespaces, reasset_bits_orphan_get_packed_namespaces());

    s32 numPackedBitNamespaces = list_get_length(&packedBitNamespaces);
    for (s32 i = 0; i < numPackedBitNamespaces; i++) {
        PackedBitRanges *ranges = list_get(&packedBitNamespaces, i);
        extension_set_add(&extensions, ranges->namespace);
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
        extHeader->sizeBytes = 0; // TODO:
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

    if (ptr > flashEndPtr) {
        recomp_eprintf("Overflow when writing recomp savedata! Savefile may be corrupt.");
    }

    // Clean up
    extension_set_free(&extensions);
    list_free(&packedBitNamespaces);

    recomp_printf("Wrote recomp savedata.\n");
}

void recomp_savedata_load(RecompFlashData *flash) {
    // Reset savegame specific reasset data
    reasset_bits_orphan_init();

    Savegame *savegame = &flash->base.asSave;

    RecompSaveDataHeader *header = &flash->recomp;
    if (header->magic[0] != 'R' || header->magic[1] != 'C') {
        // Missing or invalid recomp savedata
        recomp_printf("Recomp savedata not found (this is OK).\n");
        return;
    }
    if (header->version != 1) {
        recomp_eprintf("Recomp savedata has an invalid version: %d\n", header->version);
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

        // TODO: load custom savedata

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
                recomp_eprintf("Dropping orphaned bitstring region %s:%d. The bitstring is full.\n",
                    namespaceName, region->bitstring);
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

    if (ptr > flashEndPtr) {
        recomp_eprintf("Overflow when reading recomp savedata! Savefile may be corrupt.");
    }

    // Clean up
    for (s32 i = 0; i < (s32)ARRAYCOUNT(bitstringSrc); i++) {
        recomp_free(bitstringSrc[i]);
    }
    if (extNamespaces != NULL) {
        recomp_free(extNamespaces);
    }

    recomp_printf("Loaded recomp savedata.\n");
}
