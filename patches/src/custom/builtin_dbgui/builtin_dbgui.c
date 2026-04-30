#include "dbgui.h"
#include "builtin_dbgui.h"
#include "patches/builtin_dbgui/graphics_window.h"

static s32 dllsOpen = FALSE;
static s32 audioOpen = FALSE;
static s32 graphicsOpen = FALSE;
static s32 memoryOpen = FALSE;
static s32 warpOpen = FALSE;
static s32 playerCheatOpen = FALSE;
static s32 sidekickCheatOpen = FALSE;

void builtin_dbgui(void) {
    if (dbgui_begin_main_menu_bar()) {
        if (dbgui_begin_menu("Debug")) {
            dbgui_menu_item("Warp", &warpOpen);
            dbgui_menu_item("Graphics", &graphicsOpen);
            dbgui_menu_item("Memory", &memoryOpen);
            dbgui_menu_item("Audio", &audioOpen);
            dbgui_menu_item("DLLs", &dllsOpen);
            dbgui_end_menu();
        }
        if (dbgui_begin_menu("Cheats")) {
            dbgui_menu_item("Player", &playerCheatOpen);
            dbgui_menu_item("Sidekick", &sidekickCheatOpen);
            dbgui_end_menu();
        }
        dbgui_end_main_menu_bar();
    }

    if (dllsOpen) {
        dbgui_dlls_window(&dllsOpen);
    }
    if (audioOpen) {
        dbgui_audio_window(&audioOpen);
    }
    if (warpOpen) {
        dbgui_warp_window(&warpOpen);
    }
    if (graphicsOpen) {
        dbgui_graphics_window(&graphicsOpen);
    }
    if (memoryOpen) {
        dbgui_memory_window(&memoryOpen);
    }

    if (playerCheatOpen) {
        dbgui_player_cheat_window(&playerCheatOpen);
    }
    if (sidekickCheatOpen) {
        dbgui_sidekick_cheat_window(&sidekickCheatOpen);
    }
}

void builtin_dbgui_game_tick(void) {
    dbgui_cheat_game_tick();
    graphics_window_check_buffer_sizes();
}
