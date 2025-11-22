#include "dbgui.h"
#include "patches.h"
#include "patches/dll.h"
#include "recomp_dll.h"

#include "sys/dll.h"

void dbgui_dlls_window(s32 *open) {
    if (dbgui_begin("DLLs", open)) {
        s32 vanillaLoadedCount = 0;
        for (s32 i = 0; i < gLoadedDLLCount; i++) {
            if (gLoadedDLLList[i].tabidx != DLL_NONE) {
                vanillaLoadedCount++;
            }
        }

        s32 customDLLListCount;
        RecompCustomDLLState *customDLLList = recomp_get_loaded_custom_dlls(&customDLLListCount);
        s32 customLoadedCount = 0;
        for (s32 i = 0; i < customDLLListCount; i++) {
            if (customDLLList[i].id != DLL_NONE) {
                customLoadedCount++;
            }
        }

        if (dbgui_tree_node(recomp_sprintf_helper("Vanilla DLLs (%d/%d):###vanilla", vanillaLoadedCount, RECOMP_MAX_LOADED_DLLS))) {
            for (s32 i = 0; i < gLoadedDLLCount; i++) {
                DLLState *dll = &gLoadedDLLList[i];
                DLLFile *file = dll->tabidx == DLL_NONE
                    ? NULL
                    : DLL_EXPORTS_TO_FILE(dll->vtblPtr);
                const char *label = dll->tabidx == DLL_NONE
                    ? recomp_sprintf_helper("<empty slot>###%d", i)
                    : recomp_sprintf_helper("%d###%d", dll->tabidx, i);
                if (dbgui_tree_node(label)) {
                    dbgui_textf("refCount: %d", dll->refCount);
                    dbgui_textf("address: %p", file);
                    if (dll->tabidx != DLL_NONE) {
                        if (dbgui_tree_node(recomp_sprintf_helper("vtable (%d):###vtable", file->exportCount))) {
                            for (u32 k = 0; k < (file->exportCount + 1); k++) {
                                dbgui_textf("[%d] %p", k, dll->vtblPtr[k]);
                            }
                            dbgui_tree_pop();
                        }
                    }
                    dbgui_tree_pop();
                }
            }
            
            dbgui_tree_pop();
        }

        if (dbgui_tree_node(recomp_sprintf_helper("Custom DLLs (%d/%d):###custom", customLoadedCount, RECOMP_MAX_LOADED_CUSTOM_DLLS))) {
            for (s32 i = 0; i < customDLLListCount; i++) {
                RecompCustomDLLState *dll = &customDLLList[i];
                const char *label = dll->id == DLL_NONE
                    ? recomp_sprintf_helper("<empty slot>###%d", i)
                    : recomp_sprintf_helper("0x%X###%d", dll->id, i);
                if (dbgui_tree_node(label)) {
                    dbgui_textf("refCount: %d", dll->refCount);
                    dbgui_tree_pop();
                }
            }
            
            dbgui_tree_pop();
        }
    }
    dbgui_end();
}
