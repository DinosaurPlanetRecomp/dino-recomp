#include "reasset_textures.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/reasset_fst.h"
#include "reasset/reasset_iterator.h"
#include "reasset/buffer.h"
#include "reasset/list.h"
#include "reasset/bin_ptr.h"

#include "PR/ultratypes.h"
#include "sys/fs.h"
#include "sys/memory.h"

#define TEXTABENTRY_OFFSET(entry) (entry & 0x00FFFFFF)
#define TEXTABENTRY_NFRAMES(entry) ((entry & 0xFF000000) >> 24)

typedef struct {
    ReAssetID id;
    s32 numFrames;
    Buffer texture;
    BinPtr texturePtr;
} TextureEntry;

typedef struct {
    ReAssetID id;
    TextureBank bank;
    ReAssetID texID;
    BinPtr binPtr;
} TexTableEntry;

static s32 texOriginalCount[NUM_TEXTURE_BANKS];
static List texList[NUM_TEXTURE_BANKS];
static U32List texIDList[NUM_TEXTURE_BANKS];
static U32ValueHashmapHandle texMap[NUM_TEXTURE_BANKS];
static ReAssetResolveMap texResolveMap[NUM_TEXTURE_BANKS];

static s32 texTableOriginalCount;
static List texTableList; // list[TexTableEntry]
static U32List texTableIDList; // list[ReAssetID]
static U32ValueHashmapHandle texTableMap; // ReAssetID -> tex table list index
static ReAssetResolveMap texTableResolveMap;

static TexTableEntry* get_tex_table_entry(ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(texTableMap, id, &listIdx)) {
        return list_get(&texTableList, listIdx);
    }

    return NULL;
}

static TexTableEntry* get_or_create_tex_table_entry(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(texTableMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < texTableOriginalCount, 
                "[reasset] Attempted to patch out-of-bounds base texture table index: %d", idData->identifier);
        }

        u32list_add(&texTableIDList, id);

        listIdx = list_get_length(&texTableList);
        
        TexTableEntry *entry = list_add(&texTableList);
        entry->id = id;

        recomputil_u32_value_hashmap_insert(texTableMap, id, listIdx);
    }

    return list_get(&texTableList, listIdx);
}

static TextureEntry* get_tex(TextureBank bank, ReAssetID id) {
    u32 listIdx;
    if (recomputil_u32_value_hashmap_get(texMap[bank], id, &listIdx)) {
        return list_get(&texList[bank], listIdx);
    }

    return NULL;
}

static TextureEntry* get_or_create_tex(TextureBank bank, ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(texMap[bank], id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            reasset_assert(idData->identifier < texOriginalCount[bank], 
                "[reasset] Attempted to patch out-of-bounds base object index: %d", idData->identifier);
        }

        u32list_add(&texIDList[bank], id);

        listIdx = list_get_length(&texList[bank]);
        
        TextureEntry *entry = list_add(&texList[bank]);
        entry->id = id;
        buffer_init(&entry->texture, 0);

        recomputil_u32_value_hashmap_insert(texMap[bank], id, listIdx);
    }

    return list_get(&texList[bank], listIdx);
}

static void tex_list_element_free(void *element) {
    TextureEntry *entry = element;
    buffer_free(&entry->texture);
}

void reasset_textures_init(void) {
    // Textures
    s32 tabIDs[NUM_TEXTURE_BANKS] = {TEX0_TAB, TEX1_TAB};
    s32 binIDs[NUM_TEXTURE_BANKS] = {TEX0_BIN, TEX1_BIN};
    const char *resolveMapNames[NUM_TEXTURE_BANKS] = {"Texture0", "Texture1"};

    for (s32 bank = 0; bank < NUM_TEXTURE_BANKS; bank++) {
        texOriginalCount[bank] = (reasset_fst_get_file_size(tabIDs[bank]) / sizeof(u32)) - 2;
        u32 *originalTab = reasset_fst_alloc_load_file(tabIDs[bank], NULL);

        list_init(&texList[bank], sizeof(TextureEntry), texOriginalCount[bank]);
        list_set_element_free_callback(&texList[bank], tex_list_element_free);
        u32list_init(&texIDList[bank], texOriginalCount[bank]);
        texMap[bank] = recomputil_create_u32_value_hashmap();
        texResolveMap[bank] = reasset_resolve_map_create(resolveMapNames[bank]);

        // Add base textures (preserving order)
        for (s32 i = 0; i < texOriginalCount[bank]; i++) {
            ReAssetID id = reasset_base_id(i);
            TextureEntry *entry = get_or_create_tex(bank, id);

            entry->numFrames = TEXTABENTRY_NFRAMES(originalTab[i]);

            s32 offset = TEXTABENTRY_OFFSET(originalTab[i]);
            s32 size = TEXTABENTRY_OFFSET(originalTab[i + 1]) - offset;

            buffer_set_base(&entry->texture, binIDs[bank], offset, size);
        }

        recomp_free(originalTab);
    }

    // Texture Table
    texTableOriginalCount = (reasset_fst_get_file_size(TEXTABLE_BIN) / sizeof(u16)) - 1;
    s16 *originalTexTab = reasset_fst_alloc_load_file(TEXTABLE_BIN, NULL);

    list_init(&texTableList, sizeof(TexTableEntry), texTableOriginalCount);
    u32list_init(&texTableIDList, texTableOriginalCount);
    texTableMap = recomputil_create_u32_value_hashmap();
    texTableResolveMap = reasset_resolve_map_create("TexTableEntry");

    // Add base texture table entries (preserving order)
    for (s32 i = 0; i < texTableOriginalCount; i++) {
        ReAssetID id = reasset_base_id(i);
        TexTableEntry *entry = get_or_create_tex_table_entry(id);

        s32 bank = (originalTexTab[i] & 0x8000) ? TEX1 : TEX0;
        s32 texIndex = originalTexTab[i] & 0x7FFF;

        entry->bank = bank;
        entry->texID = reasset_base_id(texIndex);
    }

    recomp_free(originalTexTab);
}

static void repack_textures_internal(void) {
    s32 tabIDs[NUM_TEXTURE_BANKS] = {TEX0_TAB, TEX1_TAB};
    s32 binIDs[NUM_TEXTURE_BANKS] = {TEX0_BIN, TEX1_BIN};
    const char *bankNames[NUM_TEXTURE_BANKS] = {"TEX0", "TEX1"};

    for (s32 bank = 0; bank < NUM_TEXTURE_BANKS; bank++) {
        s32 newCount = list_get_length(&texList[bank]);

        // Calculate new TEX*.bin size
        u32 newBinSize = 0;
        for (s32 i = 0; i < newCount; i++) {
            TextureEntry *entry = list_get(&texList[bank], i);
            
            newBinSize += buffer_get_size(&entry->texture);
            newBinSize = mmAlign8(newBinSize);
        }

        // Alloc new TEX*.tab/bin
        u32 newTabSize = (newCount + 2) * sizeof(u32);
        u32 *newTab = recomp_alloc(newTabSize);
        void *newBin = recomp_alloc(newBinSize);
        bzero(newBin, newBinSize);

        // Rebuild
        s32 offset = 0;
        for (s32 i = 0; i < newCount; i++) {
            TextureEntry *entry = list_get(&texList[bank], i);
            ReAssetIDData *idData = reasset_id_lookup_data(entry->id);

            if (idData->namespace != REASSET_BASE_NAMESPACE) {
                const char *namespaceName;
                reasset_namespace_lookup_name(idData->namespace, &namespaceName);
                reasset_log_debug("[reasset] New %s entry: %s:%d\n",
                    bankNames[bank], 
                    namespaceName, idData->identifier);
            }

            reasset_resolve_map_resolve_id(texResolveMap[bank], entry->id, -1, i);

            newTab[i] = ((entry->numFrames & 0xFF) << 24) | (offset & 0x00FFFFFF);
            bin_ptr_set(&entry->texturePtr, newBin, offset, buffer_get_size(&entry->texture));
            buffer_copy_to_bin_ptr(&entry->texture, &entry->texturePtr);
            offset += buffer_get_size(&entry->texture);

            offset = mmAlign8(offset);
        }

        newTab[newCount] = offset;
        newTab[newCount + 1] = -1;

        // Finalize resolve map
        reasset_resolve_map_finalize(texResolveMap[bank]);

        // Set new files
        reasset_fst_set_internal(tabIDs[bank], newTab, newTabSize, /*ownedByReAsset=*/TRUE);
        reasset_fst_set_internal(binIDs[bank], newBin, newBinSize, /*ownedByReAsset=*/TRUE);
        reasset_log_info("[reasset] Rebuilt %s.tab & %s.bin (count: %d, bin size: 0x%X).\n", 
            bankNames[bank], bankNames[bank], newCount, newBinSize);
    }
}

static void repack_texture_table_internal(void) {
    s32 newCount = list_get_length(&texTableList);

    // Calculate new TEXTABLE.bin size
    u32 newBinSize = (newCount + 1) * sizeof(u16);

    // Alloc new TEXTABLE.bin
    s16 *newBin = recomp_alloc(newBinSize);
    bzero(newBin, newBinSize);

    // Rebuild
    for (s32 i = 0; i < newCount; i++) {
        TexTableEntry *entry = list_get(&texTableList, i);
        ReAssetIDData *idData = reasset_id_lookup_data(entry->id);

        if (idData->namespace != REASSET_BASE_NAMESPACE) {
            const char *namespaceName;
            reasset_namespace_lookup_name(idData->namespace, &namespaceName);
            reasset_log_debug("[reasset] New texture table entry: %s:%d\n", 
                namespaceName, idData->identifier);
        }

        reasset_resolve_map_resolve_id(texTableResolveMap, entry->id, -1, i);
        
        bin_ptr_set(&entry->binPtr, newBin, i * sizeof(u16), sizeof(u16));

        // Note: Nothing to write to bin here. Values will be filled in during the patch stage
    }

    newBin[newCount] = 1;

    // Finalize resolve map
    reasset_resolve_map_finalize(texTableResolveMap);

    // Set new files
    reasset_fst_set_internal(TEXTABLE_BIN, newBin, newBinSize, /*ownedByReAsset=*/TRUE);
    reasset_log_info("[reasset] Rebuilt TEXTABLE.bin (count: %d, bin size: 0x%X).\n", newCount, newBinSize);
}

void reasset_textures_repack(void) {
    repack_textures_internal();
    repack_texture_table_internal();
}

void reasset_textures_patch(void) {
    // Patch in resolved texture table entries
    s32 numTexTableEntries = list_get_length(&texTableList);
    for (s32 i = 0; i < numTexTableEntries; i++) {
        TexTableEntry *entry = list_get(&texTableList, i);

        u16 *entryPtr = entry->binPtr.ptr;
        if (entryPtr == NULL) {
            continue;
        }

        s32 tabIdx = reasset_resolve_map_lookup(texResolveMap[entry->bank], entry->texID);
        if (tabIdx != -1) {
            *entryPtr = (u16)(((entry->bank & 0x1) << 15) | (tabIdx & 0x7FFF));
        } else {
            *entryPtr = 0;

            const char *namespaceName;
            s32 identifier;
            reasset_id_lookup_name(entry->id, &namespaceName, &identifier);
            const char *objNamespaceName;
            s32 objIdentifier;
            reasset_id_lookup_name(entry->id, &objNamespaceName, &objIdentifier);
            reasset_log_warning("[reasset] WARN: Failed to patch texture table entry (%s:%d) texture ID %s:%d. Texture was not defined!\n",
                namespaceName, identifier, 
                objNamespaceName, objIdentifier);
        }
    }
}

void reasset_textures_cleanup(void) {
    for (s32 bank = 0; bank < NUM_TEXTURE_BANKS; bank++) {
        list_free(&texList[bank]);
        u32list_free(&texIDList[bank]);
        recomputil_destroy_u32_value_hashmap(texMap[bank]);
    }

    list_free(&texTableList);
    u32list_free(&texTableIDList);
    recomputil_destroy_u32_value_hashmap(texTableMap);
}

// MARK: Textures

static void assert_tex_bank(const char *funcName, TextureBank bank) {
    reasset_assert(bank >= 0 && bank < NUM_TEXTURE_BANKS, 
        "[reasset:%s] Invalid texture bank: %d", funcName, bank);
}

RECOMP_EXPORT void reasset_textures_set(TextureBank bank, ReAssetID id, s32 numFrames, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_textures_set");
    assert_tex_bank("reasset_textures_set", bank);

    TextureEntry *entry = get_or_create_tex(bank, id);
    buffer_set(&entry->texture, data, sizeBytes);
    entry->numFrames = numFrames;

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log_debug("[reasset] TEX%d set: %s:%d\n", bank, namespaceName, idData->identifier);
}

RECOMP_EXPORT void* reasset_textures_get(TextureBank bank, ReAssetID id, s32 *outNumFrames, u32 *outSizeBytes) {
    reasset_assert_stage_get_call("reasset_textures_get");
    assert_tex_bank("reasset_textures_get", bank);

    TextureEntry *entry = get_tex(bank, id);
    if (entry == NULL) {
        if (outNumFrames != NULL) {
            *outNumFrames = 0;
        }
        if (outSizeBytes != NULL) {
            *outSizeBytes = 0;
        }
        return NULL;
    }

    if (outNumFrames != NULL) {
        *outNumFrames = entry->numFrames;
    }

    if (reassetStage == REASSET_STAGE_RESOLVE) {
        return bin_ptr_get(&entry->texturePtr, outSizeBytes);
    } else {
        return buffer_get(&entry->texture, outSizeBytes);
    }
}

RECOMP_EXPORT ReAssetIterator reasset_textures_create_iterator(TextureBank bank) {
    reasset_assert_stage_iterator_call("reasset_textures_create_iterator");
    assert_tex_bank("reasset_textures_create_iterator", bank);

    return reasset_iterator_create(&texIDList[bank]);
}

RECOMP_EXPORT void reasset_textures_link(TextureBank bank, ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_textures_link");
    assert_tex_bank("reasset_textures_link", bank);

    reasset_resolve_map_link(texResolveMap[bank], id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_textures_get_resolve_map(TextureBank bank) {
    reasset_assert_stage_get_resolve_map_call("reasset_textures_get_resolve_map");
    assert_tex_bank("reasset_textures_get_resolve_map", bank);

    return texResolveMap[bank];
}

_Bool reasset_textures_is_base_id(TextureBank bank, s32 id) {
    assert_tex_bank("reasset_textures_is_base_id", bank);

    return id >= 0 && id < texOriginalCount[bank];
}

// MARK: Texture Table

RECOMP_EXPORT void reasset_texture_table_set(ReAssetID id, TextureBank bank, ReAssetID texID) {
    reasset_assert_stage_set_call("reasset_texture_table_set");
    assert_tex_bank("reasset_texture_table_set", bank);

    TexTableEntry *entry = get_or_create_tex_table_entry(id);
    entry->bank = bank;
    entry->texID = texID;

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log_debug("[reasset] Texture table entry set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT ReAssetBool reasset_texture_table_get(ReAssetID id, TextureBank *outBank, ReAssetID *outTexID) {
    reasset_assert_stage_get_call("reasset_texture_table_get");

    TexTableEntry *entry = get_tex_table_entry(id);
    if (entry == NULL) {
        return FALSE;
    }

    if (outBank != NULL) {
        *outBank = entry->bank;
    }
    if (outTexID != NULL) {
        *outTexID = entry->texID;
    }

    return TRUE;
}

RECOMP_EXPORT ReAssetIterator reasset_texture_table_create_iterator(void) {
    reasset_assert_stage_iterator_call("reasset_texture_table_create_iterator");

    return reasset_iterator_create(&texTableIDList);
}

RECOMP_EXPORT void reasset_texture_table_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_texture_table_link");

    reasset_resolve_map_link(texTableResolveMap, id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_texture_table_get_resolve_map(void) {
    reasset_assert_stage_get_resolve_map_call("reasset_texture_table_get_resolve_map");

    return texTableResolveMap;
}
