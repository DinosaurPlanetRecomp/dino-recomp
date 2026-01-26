#include "dbgui.h"
#include "builtin_dbgui.h"
#include "patches/builtin_dbgui/graphics_window.h"

static s32 dllsOpen = FALSE;
static s32 audioOpen = FALSE;
static s32 graphicsOpen = FALSE;
static s32 memoryOpen = FALSE;
static s32 warpCheatOpen = FALSE;
static s32 charCheatOpen = FALSE;

void builtin_dbgui() {
    if (dbgui_begin_main_menu_bar()) {
        if (dbgui_begin_menu("Debug")) {
            dbgui_menu_item("DLLs", &dllsOpen);
            dbgui_menu_item("Audio", &audioOpen);
            dbgui_menu_item("Graphics", &graphicsOpen);
            dbgui_menu_item("Memory", &memoryOpen);
            dbgui_end_menu();
        }
        if (dbgui_begin_menu("Cheats")) {
            dbgui_menu_item("Warp", &warpCheatOpen);
            dbgui_menu_item("Character", &charCheatOpen);
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
    if (warpCheatOpen) {
        dbgui_warp_cheat_window(&warpCheatOpen);
    }
    if (charCheatOpen) {
        dbgui_character_cheat_window(&charCheatOpen);
    }
    if (graphicsOpen) {
        dbgui_graphics_window(&graphicsOpen);
    }
    if (memoryOpen) {
        dbgui_memory_window(&memoryOpen);
    }
}

void builtin_dbgui_game_tick() {
    dbgui_character_cheat_game_tick();
    graphics_window_check_buffer_sizes();
}
