#pragma once

#include "PR/ultratypes.h"

#define MENU_LAST_VANILLA_ID 17

s32 recomp_add_new_menu(void);
s32 recomp_set_menu_dll_id(s32 menuID, u16 dllID);
u32 recomp_get_custom_menu_dll_id(s32 menuID);
