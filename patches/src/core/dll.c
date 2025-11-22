#include "patches.h"
#include "patches/dll.h"
#include "recomp_funcs.h"
#include "recomp_dll.h"

#include "sys/asset_thread.h"
#include "sys/dll.h"
#include "sys/fs.h"
#include "sys/memory.h"

extern DLLFile *dll_load_from_tab(u16 tabidx, s32 *sizeOut);

RECOMP_PATCH void init_dll_system() {
    s32 dllNone = DLL_NONE;

    queue_alloc_load_file((void**)&gFile_DLLS_TAB, DLLS_TAB);
    queue_alloc_load_file((void**)&gFile_DLLSIMPORTTAB, DLLSIMPORTTAB_BIN);

    gDLLCount = 2; 
    while (((DLLTabEntry*)((u8*)gFile_DLLS_TAB + gDLLCount * 2 * 4u))->offset != -1) {
        ++gDLLCount;
    }

    // @recomp: Use new DLL list size
    gLoadedDLLList = (DLLState*)mmAlloc(sizeof(DLLState) * RECOMP_MAX_LOADED_DLLS, ALLOC_TAG_DLL_COL, NULL);

    gLoadedDLLCount = RECOMP_MAX_LOADED_DLLS;
    while (gLoadedDLLCount != 0) {
        --gLoadedDLLCount;
        gLoadedDLLList[gLoadedDLLCount].tabidx = dllNone;
    }

    // @recomp: Init recomp custom DLL system
    recomp_dll_init_system();
}

RECOMP_PATCH void *dll_load_deferred(u16 idOrIdx, u16 exportCount) {
    DLLFile *dll;
    DLLState *state;
    void *dllInterfacePtr;

    dllInterfacePtr = NULL;
    
    if (!idOrIdx) {
        return NULL;
    }

    // @recomp: Redirect to custom load implementation for custom DLLs registered with recomp
    //          Note: Custom engine DLLs are not supported
    if (idOrIdx >= 0x8000) {
        if (idOrIdx > DLLBANK4_LAST_VANILLA_ID) {
            return recomp_dll_load_deferred_custom(DLL_BANK_OBJECTS, idOrIdx, exportCount);
        }
    } else if (idOrIdx >= 0x4000) {
        // recomp bank
        return recomp_dll_load_deferred_custom(DLL_BANK_RECOMP, idOrIdx, exportCount);
    } else if (idOrIdx >= 0x2000) {
        if (idOrIdx > DLLBANK2_LAST_VANILLA_ID) {
            return recomp_dll_load_deferred_custom(DLL_BANK_PROJGFX, idOrIdx, exportCount);
        }
    } else if (idOrIdx >= 0x1000) {
        if (idOrIdx > DLLBANK1_LAST_VANILLA_ID) {
            return recomp_dll_load_deferred_custom(DLL_BANK_MODGFX, idOrIdx, exportCount);
        }
    }

    queue_load_dll(&dllInterfacePtr, idOrIdx, exportCount);

    state = DLL_INTERFACE_TO_STATE(dllInterfacePtr);

    if (state->refCount == 1) {
        // A DLL interface is a pointer to a DLL state exports field.
        // Dereferencing the state exports field gives us a pointer to the DLL file exports array.
        // Using the file exports array address, we can get the DLL file instance.
        dll = DLL_EXPORTS_TO_FILE(*(void**)dllInterfacePtr);
        dll->ctor(dll);
    }

    return dllInterfacePtr;
}

RECOMP_PATCH void *dll_load(u16 idOrIdx, u16 exportCount, s32 bRunConstructor) {
    DLLFile *dll;
    u32 i;
    s32 totalSize;
    u32 **interfacePtr;

    // @recomp: Redirect to custom load implementation for custom DLLs registered with recomp
    //          Note: Custom engine DLLs are not supported
    if (idOrIdx >= 0x8000) {
        if (idOrIdx > DLLBANK4_LAST_VANILLA_ID) {
            return recomp_dll_load_custom(DLL_BANK_OBJECTS, idOrIdx, exportCount, bRunConstructor);
        }
        idOrIdx -= 0x8000;
        // bank4
        idOrIdx += gFile_DLLS_TAB->header.bank4;
    } else if (idOrIdx >= 0x4000) {
        // recomp bank
        return recomp_dll_load_custom(DLL_BANK_RECOMP, idOrIdx, exportCount, bRunConstructor);
    } else if (idOrIdx >= 0x2000) {
        if (idOrIdx > DLLBANK2_LAST_VANILLA_ID) {
            return recomp_dll_load_custom(DLL_BANK_PROJGFX, idOrIdx, exportCount, bRunConstructor);
        }
        idOrIdx -= 0x2000;
        // bank2
        idOrIdx += gFile_DLLS_TAB->header.bank2 + 1;
    } else if (idOrIdx >= 0x1000) {
        if (idOrIdx > DLLBANK1_LAST_VANILLA_ID) {
            return recomp_dll_load_custom(DLL_BANK_MODGFX, idOrIdx, exportCount, bRunConstructor);
        }
        idOrIdx -= 0x1000;
        // bank1
        idOrIdx += gFile_DLLS_TAB->header.bank1 + 1;
    }

    // Check if DLL is already loaded, and if so, increment the reference count
    for (i = 0; i < (u32)gLoadedDLLCount; i++) {
        if (idOrIdx == gLoadedDLLList[i].tabidx) {
            ++gLoadedDLLList[i].refCount;
            return &gLoadedDLLList[i].vtblPtr;
        }
    }

    dll = dll_load_from_tab(idOrIdx, &totalSize);
    if (!dll) {
        return NULL;
    }

    if (dll->exportCount < exportCount) {
        // @recomp: Error print
        recomp_eprintf("Failed to load DLL %u! Expected at least %u exports, found %u.\n", 
            idOrIdx, exportCount, dll->exportCount);
        mmFree(dll);
        return NULL;
    }

    // Find an open slot in the DLL list
    for (i = 0; i < (u32)gLoadedDLLCount; i++) {
        if (gLoadedDLLList[i].tabidx == DLL_NONE) {
            break;
        }
    }
    
    // If no open slots were available, try to add a new slot
    if (i == (u32)gLoadedDLLCount) {
        if (gLoadedDLLCount == RECOMP_MAX_LOADED_DLLS) {
            // @recomp: Error print
            recomp_eprintf("Failed to load DLL %u! Too many DLLs are already loaded.\n", idOrIdx);
            mmFree(dll);
            return NULL;
        }

        ++gLoadedDLLCount;
    }

    // @recomp: Notify recomp runtime that we loaded a DLL
    recomp_on_dll_load(idOrIdx, (void*)((u32)dll + dll->code));

    gLoadedDLLList[i].tabidx = idOrIdx;
    // The relocated export table is the vtable of the DLL at runtime
    gLoadedDLLList[i].vtblPtr = DLL_FILE_TO_EXPORTS(dll);
    gLoadedDLLList[i].end = (void*)((u32)dll + totalSize);
    gLoadedDLLList[i].refCount = 1;
    // A pointer to the vtable pointer is the interface of the DLL
    interfacePtr = &gLoadedDLLList[i].vtblPtr;

    if (bRunConstructor) {
        dll->ctor(dll);
    }

    return (void*)interfacePtr;
}

RECOMP_PATCH s32 dll_unload(void *dllInterfacePtr) {
    DLLFile *dll;
    u16 idx;
    u32 *dllClearAddr;
    u32 *dllTextEnd;

    // @recomp: Redirect to a custom unload implementation for custom DLLs registered with recomp
    RecompCustomDLLState *customDLLState = recomp_dll_interface_to_custom_state(dllInterfacePtr);
    if (customDLLState != NULL) {
        return recomp_dll_unload_custom(customDLLState);
    }

    idx = (u32)DLL_INTERFACE_TO_STATE(dllInterfacePtr) - (u32)gLoadedDLLList;

    if ((idx % 16) != 0) {
        return FALSE;
    }

    idx >>= 4;

    if (idx >= gLoadedDLLCount) {
        return FALSE;
    }

    gLoadedDLLList[idx].refCount--;

    if (gLoadedDLLList[idx].refCount == 0) {
        dll = DLL_EXPORTS_TO_FILE(gLoadedDLLList[idx].vtblPtr);
        
        dll->dtor(dll);

        dllClearAddr = gLoadedDLLList[idx].vtblPtr;
        dllTextEnd = gLoadedDLLList[idx].end;

        while (dllClearAddr < dllTextEnd) {
            *(dllClearAddr++) = 0x0007000D; // MIPS break instruction
        }

        // @recomp: Notify runtime that we're unloading a DLL
        recomp_on_dll_unload(gLoadedDLLList[idx].tabidx);

        mmFree(dll);

        gLoadedDLLList[idx].tabidx = DLL_NONE;

        while (gLoadedDLLCount != 0) {
            if (gLoadedDLLList[gLoadedDLLCount - 1].tabidx != DLL_NONE) {
                break;
            }

            gLoadedDLLCount--;
        }

        return TRUE;
    }

    return FALSE;
}
