#include "dbgui.h"
#include "patches.h"
#include "sys/dll.h"

void dbgui_dlls_window(s32 *open) {
    if (dbgui_begin("DLLs", open)) {
        s32 loaded_count = 0;
        for (s32 i = 0; i < gLoadedDLLCount; i++) {
            if (gLoadedDLLList[i].id != DLL_NONE) {
                loaded_count++;
            }
        }

        dbgui_textf("Loaded DLLs (%d):", loaded_count);

        if (dbgui_begin_child("##dlls")) {
            for (s32 i = 0; i < gLoadedDLLCount; i++) {
                DLLState *dll = &gLoadedDLLList[i];
                DLLFile *file = dll->id == DLL_NONE
                    ? NULL
                    : DLL_EXPORTS_TO_FILE(dll->vtblPtr);
                const char *label = dll->id == DLL_NONE
                    ? recomp_sprintf_helper("<empty slot>##%d", i)
                    : recomp_sprintf_helper("%d##%d", dll->id, i);
                if (dbgui_tree_node(label)) {
                    dbgui_textf("refCount: %d", dll->refCount);
                    dbgui_textf("address: %p", file);
                    if (dll->id != DLL_NONE) {
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
        }
        dbgui_end_child();
    }
    dbgui_end();
}
