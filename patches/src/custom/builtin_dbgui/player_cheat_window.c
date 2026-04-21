#include "dbgui.h"

#include "dlls/engine/29_gplay.h"
#include "dll.h"

static s32 infiniteHealth = FALSE;
static s32 infiniteMagic = FALSE;

void dbgui_player_cheat_window(s32 *open) {
    PlayerStats *char_stats = gDLL_29_Gplay->vtbl->get_player_stats();

    if (dbgui_begin("Stat Cheats", open)) {
        dbgui_checkbox("Infinite Health", &infiniteHealth);
        dbgui_checkbox("Infinite Magic", &infiniteMagic);

        s32 hp = char_stats->health;
        if (dbgui_input_int("Health", &hp)) {
            if (hp < 0) hp = 0;
            char_stats->health = (u8)hp;
        }
        s32 hpMax = char_stats->healthMax;
        if (dbgui_input_int("Max Health", &hpMax)) {
            if (hpMax < 0) hpMax = 0;
            char_stats->healthMax = (u8)hpMax;
        }
        s32 magic = char_stats->magic;
        if (dbgui_input_int("Magic", &magic)) {
            if (magic < 0) magic = 0;
            char_stats->magic = (s16)magic;
        }
        s32 magicMax = char_stats->magicMax;
        if (dbgui_input_int("Max Magic", &magicMax)) {
            if (magicMax < 0) magicMax = 0;
            char_stats->magicMax = (s16)magicMax;
        }
        s32 scarabs = char_stats->scarabs;
        if (dbgui_input_int("Scarabs", &scarabs)) {
            if (scarabs < 0) scarabs = 0;
            char_stats->scarabs = (s16)scarabs;
        }
    }
    dbgui_end();
}

void dbgui_player_cheat_game_tick() {
    PlayerStats *char_stats = gDLL_29_Gplay->vtbl->get_player_stats();

    if (infiniteHealth) {
        char_stats->health = char_stats->healthMax;
    }
    if (infiniteMagic) {
        char_stats->magic = char_stats->magicMax;
    }
}
