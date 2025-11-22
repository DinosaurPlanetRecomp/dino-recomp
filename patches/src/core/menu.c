#include "patches.h"
#include "recomp_menu.h"

#include "sys/dll.h"
#include "sys/fonts.h"
#include "sys/menu.h"

extern s32 gMenuDLLIDs[18];

extern s32 D_800A7D50;
extern DLL_IMenu *gActiveMenuDLL;
extern s32 gNextMenuID;
extern s32 gCurrentMenuID;
extern s32 gPreviousMenuID;

RECOMP_PATCH void menu_do_menu_swap() {
    if (gNextMenuID != 0) {
        gNextMenuID -= 1;

        if (gNextMenuID == MENU_POST) {
            D_800A7D50 = 0;
        }

        if (gCurrentMenuID == MENU_NONE) {
            font_window_flush_strings(1);
        }

        gPreviousMenuID = gCurrentMenuID;

        if (gActiveMenuDLL != NULL) {
            dll_unload(gActiveMenuDLL);
            gActiveMenuDLL = NULL;
        }

        // @recomp: Load custom menu DLLs registered with recomp, if not a vanilla menu ID
        if (gNextMenuID >= (s32)ARRAYCOUNT(gMenuDLLIDs)) {
            gActiveMenuDLL = (DLL_IMenu*)dll_load_deferred(recomp_get_custom_menu_dll_id(gNextMenuID), 1);
        } else if (gMenuDLLIDs[gNextMenuID] != -1) {
            gActiveMenuDLL = (DLL_IMenu*)dll_load_deferred(gMenuDLLIDs[gNextMenuID], 1);
        } else {
            gActiveMenuDLL = NULL;
            gNextMenuID = 0;
        }

        gCurrentMenuID = gNextMenuID;
        gNextMenuID = 0;
    }
}
