#include "patches.h"
#include "dbgui.h"
#include "builtin_dbgui.h"
#include "ui_funcs.h"

RECOMP_DECLARE_EVENT(recomp_on_game_tick_start());
RECOMP_DECLARE_EVENT(recomp_on_game_tick());
RECOMP_DECLARE_EVENT(recomp_on_game_tick_end());
RECOMP_DECLARE_EVENT(recomp_on_dbgui());

static void dbgui();

void game_tick_start_hook() {
    recomp_on_game_tick_start();
}

void game_tick_hook() {
    recomp_on_game_tick();

    recomp_run_ui_callbacks();
    dbgui();
}

void game_tick_end_hook() {
    recomp_on_game_tick_end();
}

static void dbgui() {
    dbgui_ui_frame_begin();
    
    if (dbgui_is_open()) {
        builtin_dbgui();
        recomp_on_dbgui();

        if (dbgui_begin_main_menu_bar()) {
            dbgui_text("| Press ` or F9 to close debug UI.");
            dbgui_end_main_menu_bar();
        }
    }

    dbgui_ui_frame_end();

    builtin_dbgui_game_tick();
}
