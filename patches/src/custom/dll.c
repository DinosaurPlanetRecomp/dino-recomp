#include "patches.h"
#include "recompdata.h"
#include "recomp_dll.h"
#include "recomp_funcs.h"

#include "PR/ultratypes.h"
#include "sys/asset_thread.h"
#include "sys/dll.h"

// Note: Custom engine DLLs are not supported as their tab indices and IDs are ambigious. If the
//       game requests a tab index for a non-engine DLL, it will overlap with any custom engine
//       DLL IDs. This hopefully doesn't happen in practice but we disallow custom engine DLLs
//       anyway just to avoid the issue entirely. The new 'recomp' bank should be used instead.

struct RecompCustomDLL;

typedef void (*RecompDLLFunc)(struct RecompCustomDLL *self);

typedef struct RecompCustomDLL {
    RecompDLLFunc ctor;
    RecompDLLFunc dtor;
    u16 exportCount;
    void *vtblPtr;
} RecompCustomDLL;

static struct {
    u16 bank1;
    u16 bank2;
    u16 bank3;
    u16 bank4;
} recompNextCustomDLLIDs = {
    .bank1 = DLLBANK1_LAST_VANILLA_ID + 1,
    .bank2 = DLLBANK2_LAST_VANILLA_ID + 1,
    .bank3 = 0x4000,
    .bank4 = DLLBANK4_LAST_VANILLA_ID + 1
};
static struct {
    _Bool bInitialized;
    U32MemoryHashmapHandle bank1; 
    U32MemoryHashmapHandle bank2; 
    U32MemoryHashmapHandle bank3; 
    U32MemoryHashmapHandle bank4; 
} recompCustomDLLTab = { 
    .bInitialized = FALSE
};

static RecompCustomDLLState recompCustomDLLList[RECOMP_MAX_LOADED_CUSTOM_DLLS];
static s32 recompLoadedCustomDLLCount = 0;

RECOMP_EXPORT u16 recomp_register_dll(RecompDLLBank bank, u16 exportCount, RecompDLLFunc ctor, RecompDLLFunc dtor, void *vtblPtr) {
    if (bank == DLL_BANK_ENGINE) {
        recomp_exit_with_error("[recomp_register_dll] Custom engine DLLs are not supported. Consider using the recomp bank instead.");
        return 0;
    }
    if (ctor == NULL) {
        recomp_exit_with_error("[recomp_register_dll] Constructor pointer cannot be null!");
        return 0;
    }
    if (dtor == NULL) {
        recomp_exit_with_error("[recomp_register_dll] Destructor pointer cannot be null!");
        return 0;
    }
    if (vtblPtr == NULL) {
        recomp_exit_with_error("[recomp_register_dll] Vtable pointer cannot be null!");
        return 0;
    }
    
    // Lazy init hashmaps
    if (!recompCustomDLLTab.bInitialized) {
        recompCustomDLLTab.bank1 = recomputil_create_u32_memory_hashmap(sizeof(RecompCustomDLL));
        recompCustomDLLTab.bank2 = recomputil_create_u32_memory_hashmap(sizeof(RecompCustomDLL));
        recompCustomDLLTab.bank3 = recomputil_create_u32_memory_hashmap(sizeof(RecompCustomDLL));
        recompCustomDLLTab.bank4 = recomputil_create_u32_memory_hashmap(sizeof(RecompCustomDLL));
        recompCustomDLLTab.bInitialized = TRUE;
    }

    // Generate an ID and grab the appropriate bank hashmap
    u16 id;
    U32MemoryHashmapHandle bankDLLMap;
    switch (bank) {
        case 1:
            if (recompNextCustomDLLIDs.bank1 == 0x2000) {
                recomp_exit_with_error("[recomp_register_dll] Bank 1 is full!");
                return 0;
            }
            id = recompNextCustomDLLIDs.bank1++;
            bankDLLMap = recompCustomDLLTab.bank1;
            break;
        case 2:
            if (recompNextCustomDLLIDs.bank2 == 0x4000) {
                recomp_exit_with_error("[recomp_register_dll] Bank 2 is full!");
                return 0;
            }
            id = recompNextCustomDLLIDs.bank2++;
            bankDLLMap = recompCustomDLLTab.bank2;
            break;
        case 3:
            if (recompNextCustomDLLIDs.bank3 == 0x8000) {
                recomp_exit_with_error("[recomp_register_dll] Bank 3 is full!");
                return 0;
            }
            id = recompNextCustomDLLIDs.bank3++;
            bankDLLMap = recompCustomDLLTab.bank3;
            break;
        case 4:
            if (recompNextCustomDLLIDs.bank4 == 0xFFFF) {
                recomp_exit_with_error("[recomp_register_dll] Bank 4 is full!");
                return 0;
            }
            id = recompNextCustomDLLIDs.bank4++;
            bankDLLMap = recompCustomDLLTab.bank4;
            break;
        default:
            recomp_exit_with_error(recomp_sprintf_helper("[recomp_register_dll] Invalid bank: %d", bank));
            return 0;
    }

    // Insert the new DLL into the hashmap
    if (!recomputil_u32_memory_hashmap_create(bankDLLMap, id)) {
        recomp_exit_with_error(recomp_sprintf_helper(
            "[recomp_register_dll] Hashmap error. DLL 0x%X already exists in bank %d.", id, bank));
    }
    RecompCustomDLL *dll = recomputil_u32_memory_hashmap_get(bankDLLMap, id);
    if (dll == NULL) {
        // Shouldn't ever happen...
        recomp_exit_with_error(recomp_sprintf_helper(
            "[recomp_register_dll] Hashmap error. Failed to get DLL 0x%X from bank %d hashmap.", id, bank));
    }

    dll->ctor = ctor;
    dll->dtor = dtor;
    dll->vtblPtr = vtblPtr;
    dll->exportCount = exportCount;

    if (recomp_get_debug_dll_logging_enabled()) {
        recomp_printf("Registered custom DLL 0x%X.\n", id);
    }

    return id;
}

void recomp_dll_init_system(void) {
    for (u32 i = 0; i < ARRAYCOUNT(recompCustomDLLList); i++) {
        recompCustomDLLList[i].id = DLL_NONE;
    }
}

RecompCustomDLLState *recomp_get_loaded_custom_dlls(s32 *outLoadedDLLCount) {
    *outLoadedDLLCount = recompLoadedCustomDLLCount;
    return recompCustomDLLList;
}

void *recomp_dll_load_deferred_custom(RecompDLLBank bank, u16 id, u16 exportCount) {
    RecompCustomDLLState *state;
    void *dllInterfacePtr = NULL;
    queue_load_dll(&dllInterfacePtr, id, exportCount);

    state = DLL_INTERFACE_TO_CUSTOM_STATE(dllInterfacePtr);

    if (state->refCount == 1) {
        // Get from custom DLL bank hashmap
        RecompCustomDLL *dll = NULL;
        if (recompCustomDLLTab.bInitialized) {
            switch (bank) {
                case 1:
                    dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank1, id);
                    break;
                case 2:
                    dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank2, id);
                    break;
                case 3:
                    dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank3, id);
                    break;
                case 4:
                    dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank4, id);
                    break;
                default:
                    recomp_exit_with_error(recomp_sprintf_helper("[recomp_dll_load_deferred_custom] Invalid bank: %d", bank));
                    return NULL;
            }
        }

        if (dll == NULL) {
            recomp_exit_with_error(recomp_sprintf_helper(
                "[recomp_dll_load_deferred_custom] Load failed. Custom DLL ID 0x%X not found in bank %d.", id, bank));
            return NULL;
        }

        dll->ctor(dll);
    }

    return dllInterfacePtr;
}

void *recomp_dll_load_custom(RecompDLLBank bank, u16 id, u16 exportCount, s32 bRunConstructor) {
    // Check if DLL is already loaded, and if so, increment the reference count
    for (u32 i = 0; i < (u32)recompLoadedCustomDLLCount; i++) {
        if (id == recompCustomDLLList[i].id) {
            ++recompCustomDLLList[i].refCount;
            return &recompCustomDLLList[i].vtblPtr;
        }
    }

    // Get from custom DLL bank hashmap
    RecompCustomDLL *dll = NULL;
    if (recompCustomDLLTab.bInitialized) {
        switch (bank) {
            case 1:
                dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank1, id);
                break;
            case 2:
                dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank2, id);
                break;
            case 3:
                dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank3, id);
                break;
            case 4:
                dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank4, id);
                break;
            default:
                recomp_exit_with_error(recomp_sprintf_helper("[recomp_dll_load_custom] Invalid bank: %d", bank));
                return NULL;
        }
    }

    if (dll == NULL) {
        recomp_exit_with_error(recomp_sprintf_helper(
            "[recomp_dll_load_custom] Load failed. Custom DLL ID 0x%X not found in bank %d.", id, bank));
        return NULL;
    }

    if (dll->exportCount < exportCount) {
        recomp_eprintf("Failed to load custom DLL 0x%X! Expected at least %u exports, found %u.\n", 
            id, exportCount, dll->exportCount);
        return NULL;
    }

    // Find an open slot in the DLL list
    u32 i;
    for (i = 0; i < (u32)recompLoadedCustomDLLCount; i++) {
        if (recompCustomDLLList[i].id == DLL_NONE) {
            break;
        }
    }
    
    // If no open slots were available, try to add a new slot
    if (i == (u32)recompLoadedCustomDLLCount) {
        if (recompLoadedCustomDLLCount == MAX_LOADED_DLLS) {
            recomp_eprintf("Failed to load custom DLL 0x%X! Too many DLLs are already loaded.\n", id);
            return NULL;
        }

        ++recompLoadedCustomDLLCount;
    }

    if (recomp_get_debug_dll_logging_enabled()) {
        recomp_printf("Loaded custom DLL 0x%X\n", id);
    }

    recompCustomDLLList[i].id = id;
    recompCustomDLLList[i].vtblPtr = dll->vtblPtr;
    recompCustomDLLList[i].refCount = 1;
    // A pointer to the vtable pointer is the interface of the DLL
    void **interfacePtr = &recompCustomDLLList[i].vtblPtr;

    if (bRunConstructor) {
        dll->ctor(dll);
    }

    return (void*)interfacePtr;
}

RecompCustomDLLState *recomp_dll_interface_to_custom_state(void *dllInterfacePtr) {
    u32 idx = (u32)DLL_INTERFACE_TO_CUSTOM_STATE(dllInterfacePtr) - (u32)recompCustomDLLList;

    if ((idx % sizeof(RecompCustomDLLState)) != 0) {
        return NULL;
    }

    idx /= sizeof(RecompCustomDLLState);

    if (idx < 0) {
        return NULL;
    }
    if (idx >= (u32)recompLoadedCustomDLLCount) {
        return NULL;
    }

    return &recompCustomDLLList[idx];
}

s32 recomp_dll_unload_custom(RecompCustomDLLState *state) {
    RecompCustomDLL *dll = NULL;
    s32 bank = -1;
    if (state->id >= 0x8000) {
        bank = 4;
        dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank4, state->id);
    } else if (state->id >= 0x4000) {
        bank = 3;
        dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank3, state->id);
    } else if (state->id >= 0x2000) {
        bank = 2;
        dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank2, state->id);
    } else if (state->id >= 0x1000) {
        bank = 1;
        dll = recomputil_u32_memory_hashmap_get(recompCustomDLLTab.bank1, state->id);
    } else {
        recomp_exit_with_error("[recomp_dll_unload_custom] Invalid bank: 0");
    }

    if (dll == NULL) {
        recomp_exit_with_error(recomp_sprintf_helper(
            "[recomp_dll_unload_custom] Unload error. Custom DLL ID 0x%X was not found in bank %d.", state->id, bank));
        return TRUE;
    }
        
    dll->dtor(dll);

    if (recomp_get_debug_dll_logging_enabled()) {
        recomp_printf("Unloaded custom DLL 0x%X\n", state->id);
    }

    state->id = DLL_NONE;

    while (recompLoadedCustomDLLCount != 0) {
        if (recompCustomDLLList[recompLoadedCustomDLLCount - 1].id != DLL_NONE) {
            break;
        }

        recompLoadedCustomDLLCount--;
    }

    return TRUE;
}
