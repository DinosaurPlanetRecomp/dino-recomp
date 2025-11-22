#include "patches.h"
#include "recomp_funcs.h"

#include "sys/dll.h"
#include "sys/memory.h"

extern DLLFile *dll_load_from_tab(u16 id, s32 *sizeOut);

RECOMP_PATCH void *dll_load(u16 id, u16 exportCount, s32 runConstructor) {
    DLLFile *dll;
    u32 i;
    s32 totalSize;
    u32 **interfacePtr;

    if (id >= 0x8000) {
        id -= 0x8000;
        // bank3
        id += gFile_DLLS_TAB->header.bank3;
    } else if (id >= 0x2000) {
        id -= 0x2000;
        // bank2
        id += gFile_DLLS_TAB->header.bank2 + 1;
    } else if (id >= 0x1000) {
        id -= 0x1000;
        // bank1
        id += gFile_DLLS_TAB->header.bank1 + 1;
    }

    // Check if DLL is already loaded, and if so, increment the reference count
    for (i = 0; i < (u32)gLoadedDLLCount; i++) {
        if (id == gLoadedDLLList[i].id) {
            ++gLoadedDLLList[i].refCount;
            return &gLoadedDLLList[i].vtblPtr;
        }
    }

    dll = dll_load_from_tab(id, &totalSize);
    if (!dll) {
        return NULL;
    }
    
    if (dll->exportCount < exportCount) {
        // @recomp: Error print
        recomp_eprintf("Failed to load DLL %u! Expected at least %u exports, found %u.\n", 
            id, exportCount, dll->exportCount);
        mmFree(dll);
        return NULL;
    }

    // Find an open slot in the DLL list
    for (i = 0; i < (u32)gLoadedDLLCount; i++) {
        if (gLoadedDLLList[i].id == DLL_NONE) {
            break;
        }
    }
    
    // If no open slots were available, try to add a new slot
    if (i == (u32)gLoadedDLLCount) {
        if (gLoadedDLLCount == MAX_LOADED_DLLS) {
            // @recomp: Error print
            recomp_eprintf("Failed to load DLL %u! Too many DLLs are already loaded.\n", id);
            mmFree(dll);
            return NULL;
        }

        ++gLoadedDLLCount;
    }

    // @recomp: Notify recomp runtime that we loaded a DLL
    recomp_on_dll_load(id, (void*)((u32)dll + dll->code));

    gLoadedDLLList[i].id = id;
    // The relocated export table is the vtable of the DLL at runtime
    gLoadedDLLList[i].vtblPtr = DLL_FILE_TO_EXPORTS(dll);
    gLoadedDLLList[i].end = (void*)((u32)dll + totalSize);
    gLoadedDLLList[i].refCount = 1;
    // A pointer to the vtable pointer is the interface of the DLL
    interfacePtr = &gLoadedDLLList[i].vtblPtr;

    if (runConstructor) {
        dll->ctor(dll);
    }

    return (void*)interfacePtr;
}

RECOMP_PATCH s32 dll_unload(void *dllInterfacePtr) {
    DLLFile *dll;
    u16 idx;
    u32 *dllClearAddr;
    u32 *dllTextEnd;

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
        recomp_on_dll_unload(gLoadedDLLList[idx].id);

        mmFree(dll);

        gLoadedDLLList[idx].id = DLL_NONE;

        while (gLoadedDLLCount != 0) {
            if (gLoadedDLLList[gLoadedDLLCount - 1].id != DLL_NONE) {
                break;
            }

            gLoadedDLLCount--;
        }

        return TRUE;
    }

    return FALSE;
}
