#include "reasset_dlls.h"

#include "patches.h"
#include "recompdata.h"
#include "reasset.h"
#include "reasset/reasset_id.h"
#include "reasset/reasset_resolve_map.h"
#include "reasset/reasset_namespace.h"
#include "reasset/list.h"
#include "recomp_dll.h"

#include "PR/ultratypes.h"

typedef struct {
    ReAssetID id;
    RecompDLLBank bank;
    u16 exportCount;
    RecompDLLFunc ctor;
    RecompDLLFunc dtor;
    void *vtblPtr;
} DLLEntry;

static List dllList; // list[DLLEntry]
static U32ValueHashmapHandle dllMap; // ReAssetID -> dll list index
static ReAssetResolveMap dllResolveMap;

static DLLEntry* get_or_create_dll(ReAssetID id) {
    u32 listIdx;
    if (!recomputil_u32_value_hashmap_get(dllMap, id, &listIdx)) {
        ReAssetIDData *idData = reasset_id_lookup_data(id);

        listIdx = list_get_length(&dllList);
        
        DLLEntry *entry = list_add(&dllList);
        entry->id = id;

        recomputil_u32_value_hashmap_insert(dllMap, id, listIdx);
    }

    return list_get(&dllList, listIdx);
}

void reasset_dlls_init(void) {
    list_init(&dllList, sizeof(DLLEntry), 0);
    dllMap = recomputil_create_u32_value_hashmap();
    dllResolveMap = reasset_resolve_map_create("DLL");
}

void reasset_dlls_repack(void) {
    // Register new DLLs
    s32 newCount = list_get_length(&dllList);
    for (s32 i = 0; i < newCount; i++) {
        DLLEntry *entry = list_get(&dllList, i);

        u16 id = recomp_register_dll(entry->bank, entry->exportCount, entry->ctor, entry->dtor, entry->vtblPtr);

        reasset_resolve_map_resolve_id(dllResolveMap, entry->id, -1, id, NULL);
    }

    // Finalize resolve map
    reasset_resolve_map_finalize(dllResolveMap);

    reasset_log("[reasset] Registered %d new DLLs.\n", newCount);

    // Clean up
    list_free(&dllList);
    recomputil_destroy_u32_value_hashmap(dllMap);
}

RECOMP_EXPORT void reasset_dlls_set(ReAssetID id, RecompDLLBank bank, u16 exportCount, 
        RecompDLLFunc ctor, RecompDLLFunc dtor, void *vtblPtr) {
    reasset_assert_stage_set_call("reasset_dlls_set");

    reasset_assert(id != DLL_BANK_ENGINE, 
        "[reasset:reasset_dlls_set] Custom engine DLLs are not supported. Consider using the recomp bank instead.");
    reasset_assert(ctor != NULL, "[reasset:reasset_dlls_set] Constructor pointer cannot be null!");
    reasset_assert(dtor != NULL, "[reasset:reasset_dlls_set] Destructor pointer cannot be null!");
    reasset_assert(vtblPtr != NULL, "[reasset:reasset_dlls_set] Vtable pointer cannot be null!");

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    reasset_assert(idData->namespace != REASSET_BASE_NAMESPACE, 
        "[reasset:reasset_dlls_set] Base DLLs can not be replaced at this time.");
    
    DLLEntry *entry = get_or_create_dll(id);
    entry->bank = bank;
    entry->exportCount = exportCount;
    entry->ctor = ctor;
    entry->dtor = dtor;
    entry->vtblPtr = vtblPtr;

    const char *namespaceName;
    reasset_namespace_lookup_name(idData->namespace, &namespaceName);
    reasset_log("[reasset] DLL set: %s:%d\n", namespaceName, idData->identifier);
}

RECOMP_EXPORT void reasset_dlls_link(ReAssetID id, ReAssetID externID) {
    reasset_assert_stage_link_call("reasset_dlls_link");

    reasset_resolve_map_link(dllResolveMap, id, externID);
}

RECOMP_EXPORT s32 reasset_dlls_lookup(ReAssetID id) {
    reasset_assert_stage_get_resolve_map_call("reasset_dlls_lookup");

    ReAssetIDData *idData = reasset_id_lookup_data(id);
    if (idData->namespace == REASSET_BASE_NAMESPACE) {
        return idData->identifier;
    }

    return reasset_resolve_map_lookup(dllResolveMap, id);
}
