#include "dbgui.h"

#include "dlls/engine/29_gplay.h"
#include "dll.h"

static s32 infinite_health = FALSE;
static s32 infinite_mana = FALSE;

void dbgui_character_cheat_window(s32 *open) {
    PlayerStats *char_stats = gDLL_29_Gplay->vtbl->get_player_stats();

    if (dbgui_begin("Character Cheats", open)) {
        dbgui_checkbox("Infinite Health", &infinite_health);
        dbgui_checkbox("Infinite Mana", &infinite_mana);

        s32 hp = char_stats->health;
        if (dbgui_input_int("Health", &hp)) {
            char_stats->health = (u8)hp;
        }
        s32 hpMax = char_stats->healthMax;
        if (dbgui_input_int("Max Health", &hpMax)) {
            char_stats->healthMax = (u8)hpMax;
        }
        s32 magic = char_stats->magic;
        if (dbgui_input_int("Mana", &magic)) {
            char_stats->magic = (s16)magic;
        }
        s32 magicMax = char_stats->magicMax;
        if (dbgui_input_int("Max Mana", &magicMax)) {
            char_stats->magicMax = (s16)magicMax;
        }
        s32 scarabs = char_stats->scarabs;
        if (dbgui_input_int("Scarabs", &scarabs)) {
            char_stats->scarabs = (s16)scarabs;
        }
    }
    dbgui_end();
}

void dbgui_character_cheat_game_tick() {
    if (!dbgui_is_enabled()) {
        infinite_health = FALSE;
        infinite_mana = FALSE;
        return;
    }
    
    PlayerStats *char_stats = gDLL_29_Gplay->vtbl->get_player_stats();

    if (infinite_health) {
        char_stats->health = char_stats->healthMax;
    }
    if (infinite_mana) {
        char_stats->magic = char_stats->magicMax;
    }
}
