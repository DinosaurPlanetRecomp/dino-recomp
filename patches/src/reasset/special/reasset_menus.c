#include "reasset_menus.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/list.h"
#include "reasset/special/reasset_dlls.h"
#include "recomp_menu.h"

#include "PR/ultratypes.h"

typedef struct {
    ReAssetID id;
    ReAssetID dll;
} MenuEntry;

static List menuList; // list[MenuEntry]
static U32ValueHashmapHandle menuMap; // ReAssetID -> menu list index
static ReAssetResolveMap menuResolveMap;

static MenuEntry* get_or_create_menu(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(menuMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        listIdx = list_get_length(&menuList);
        
        MenuEntry *entry = list_add(&menuList);
        entry->id = id;

        recomputil_u32_value_hashmap_insert(menuMap, id, listIdx);
    }

    return list_get(&menuList, listIdx);
}

void reasset_menus_init(void) {
    list_init(&menuList, sizeof(MenuEntry), 0);
    menuMap = recomputil_create_u32_value_hashmap();
    menuResolveMap = reasset_resolve_map_create("Menu");
}

void reasset_menus_repack(void) {
    // Register new menus
    s32 newCount = list_get_length(&menuList);
    for (s32 i = 0; i < newCount; i++) {
        MenuEntry *entry = list_get(&menuList, i);

        s32 id = recomp_add_new_menu();

        reasset_resolve_map_resolve_id(menuResolveMap, entry->id, -1, id, NULL);
    }

    // Finalize resolve map
    reasset_resolve_map_finalize(menuResolveMap);

    reasset_log("[reasset] Registered %d new menus.\n", newCount);
}

void reasset_menus_patch(void) {
    // Patch in DLL IDs for new menus
    s32 newCount = list_get_length(&menuList);
    for (s32 i = 0; i < newCount; i++) {
        MenuEntry *entry = list_get(&menuList, i);
        ReAssetIDData *idData = reasset_id_lookup_data(entry->id);
        if (idData->namespace == REASSET_BASE_NAMESPACE) {
            continue;
        }

        s32 id = reasset_resolve_map_lookup(menuResolveMap, entry->id);
        s32 dllID = reasset_dlls_lookup(entry->dll);

        if (dllID == -1) {
            s32 identifier;
            const char *namespaceName;
            reasset_id_lookup_name(entry->id, &namespaceName, &identifier);
            s32 dllIdentifier;
            const char *dllNamespaceName;
            reasset_id_lookup_name(entry->dll, &dllNamespaceName, &dllIdentifier);

            reasset_log_warning("[reasset] WARN: DLL %s:%d for menu %s:%d was not defined!",
                dllNamespaceName, dllIdentifier,
                namespaceName, identifier);
            continue;
        }

        recomp_set_menu_dll_id(id, dllID);
    }
}

void reasset_menus_cleanup(void) {
    list_free(&menuList);
    recomputil_destroy_u32_value_hashmap(menuMap);
}

static void assert_custom_menu_id(const char *funcName, ReAssetID id) {
    ReAssetIDData *idData = reasset_id_lookup_data(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        return;
    }

    if (idData->identifier >= 0 && idData->identifier <= MENU_LAST_VANILLA_ID) {
        const char *namespaceName;
        reasset_namespace_lookup_name(idData->namespace, &namespaceName);
        reasset_error("[reasset:%s] Custom menu identifier %d (%s:%d) cannot overlap with base menu IDs. Reserved IDs: 0x0-%d",
            funcName,
            idData->identifier, namespaceName, idData->identifier,
            MENU_LAST_VANILLA_ID);
    }
}

RECOMP_EXPORT void reasset_menus_set(ReAssetID id, ReAssetID dll) {
    reasset_assert_stage_set_call("reasset_menus_set");
    assert_custom_menu_id("reasset_menus_set", id);

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    reasset_assert(idData->namespace != REASSET_BASE_NAMESPACE, 
        "[reasset:reasset_menus_set] Base menus can not be replaced at this time.");
    
    MenuEntry *entry = get_or_create_menu(id);
    entry->dll = dll;

    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] Menu set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT void reasset_menus_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_menus_link");
    assert_custom_menu_id("reasset_menus_link", id);

    reasset_resolve_map_link(menuResolveMap, id, externID);
}

RECOMP_EXPORT s32 reasset_menus_lookup(ReAssetID id) {
    reasset_assert_stage_get_resolve_map_call("reasset_menus_lookup");

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        return idData->identifier;
    }

    return reasset_resolve_map_lookup(menuResolveMap, id);
}
