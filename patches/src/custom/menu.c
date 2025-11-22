#include "patches.h"
#include "recompdata.h"
#include "recomp_funcs.h"
#include "recomp_menu.h"

#include "macros.h"

extern s32 gMenuDLLIDs[18];

static U32ValueHashmapHandle recompMenuDLLMap;
static _Bool recompMenuDLLMapInitialized = FALSE;
static s32 recompNextCustomMenuID = ARRAYCOUNT(gMenuDLLIDs);

RECOMP_EXPORT s32 recomp_register_menu(u16 dllID) {
    if (!recompMenuDLLMapInitialized) {
        recompMenuDLLMap = recomputil_create_u32_value_hashmap();
        recompMenuDLLMapInitialized = TRUE;
    }

    s32 menuID = recompNextCustomMenuID++;
    recomputil_u32_value_hashmap_insert(recompMenuDLLMap, menuID, dllID);

    if (recomp_get_debug_dll_logging_enabled()) {
        recomp_printf("Registered custom menu %d with DLL ID 0x%X.\n", menuID, dllID);
    }

    return menuID;
}

u32 recomp_get_custom_menu_dll_id(s32 menuID) {
    u32 dllID;
    _Bool found = recompMenuDLLMapInitialized && 
        recomputil_u32_value_hashmap_get(recompMenuDLLMap, menuID, &dllID);

    if (!found) {
        recomp_exit_with_error(recomp_sprintf_helper(
            "Failed to load custom menu DLL. No DLL mapped for menu ID: %d", menuID));
        return 0;
    }

    return dllID;
}
