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

#include "PR/ultratypes.h"
#include "sys/fs.h"
#include "sys/memory.h"

#define TEXTABENTRY_OFFSET(entry) (entry & 0x00FFFFFF)
#define TEXTABENTRY_NFRAMES(entry) ((entry & 0xFF000000) >> 24)

typedef enum TextureBank {
    TEX0,
    TEX1,
    NUM_TEXTURE_BANKS
} TextureBank;

typedef struct {
    ReAssetID id;
    s32 numFrames;
    Buffer texture;
} TextureEntry;

static s32 texOriginalCount[NUM_TEXTURE_BANKS];
static List texList[NUM_TEXTURE_BANKS];
static U32List texIDList[NUM_TEXTURE_BANKS];
static U32ValueHashmapHandle texMap[NUM_TEXTURE_BANKS];
static ReAssetResolveMap texResolveMap[NUM_TEXTURE_BANKS];

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
}

void reasset_textures_repack(void) {
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
                reasset_log("[reasset] New %s entry: %s:%d\n",
                    bankNames[bank], 
                    namespaceName, idData->identifier);
            }

            reasset_resolve_map_resolve_id(texResolveMap[bank], entry->id, -1, i, (u8*)newBin + offset);

            newTab[i] = ((entry->numFrames & 0xFF) << 24) | (offset & 0x00FFFFFF);
            buffer_copy_to(&entry->texture, newBin, offset);
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
        reasset_log("[reasset] Rebuilt %s.tab & %s.bin (count: %d, bin size: 0x%X).\n", 
            bankNames[bank], bankNames[bank], newCount, newBinSize);

        // Clean up
        list_free(&texList[bank]);
        u32list_free(&texIDList[bank]);
        recomputil_destroy_u32_value_hashmap(texMap[bank]);
    }
}

static void assert_custom_tex_id(const char *funcName, TextureBank bank, ReAssetID id) {
    ReAssetIDData *idData = reasset_id_lookup_data(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        return;
    }

    if (idData->identifier >= 0 && idData->identifier <= texOriginalCount[bank]) {
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_error("[reasset:%s] Custom TEX%d identifier %s:%d cannot overlap base texture IDs. Reserved IDs: 0-%d.",
            funcName, bank,
            namespaceName, idData->identifier, texOriginalCount[bank]);
    }
}

static void assert_tex_bank(const char *funcName, TextureBank bank) {
    reasset_assert(bank >= 0 && bank < NUM_TEXTURE_BANKS, 
        "[reasset:%s] Invalid texture bank: %d", funcName, bank);
}

RECOMP_EXPORT void reasset_textures_set(TextureBank bank, ReAssetID id, s32 numFrames, const void *data, u32 sizeBytes) {
    reasset_assert_stage_set_call("reasset_textures_set");
    assert_tex_bank("reasset_textures_set", bank);
    assert_custom_tex_id("reasset_textures_set", bank, id);

    TextureEntry *entry = get_or_create_tex(bank, id);
    buffer_set(&entry->texture, data, sizeBytes);
    entry->numFrames = numFrames;

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] TEX%d set: %s:%d\n", bank, namespaceName, idData->identifier);
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

    return buffer_get(&entry->texture, outSizeBytes);
}

RECOMP_EXPORT ReAssetIterator reasset_textures_create_iterator(TextureBank bank) {
    reasset_assert_stage_iterator_call("reasset_textures_create_iterator");
    assert_tex_bank("reasset_textures_create_iterator", bank);

    return reasset_iterator_create(&texIDList[bank]);
}

RECOMP_EXPORT void reasset_textures_link(TextureBank bank, ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_textures_link");
    assert_tex_bank("reasset_textures_link", bank);
    assert_custom_tex_id("reasset_textures_link", bank, id);

    reasset_resolve_map_link(texResolveMap[bank], id, externID);
}

RECOMP_EXPORT ReAssetResolveMap reasset_textures_get_resolve_map(TextureBank bank) {
    reasset_assert_stage_get_resolve_map_call("reasset_textures_get_resolve_map");
    assert_tex_bank("reasset_textures_get_resolve_map", bank);

    return texResolveMap[bank];
}
