#include "dbgui.h"
#include "patches.h"

#include "dll.h"
#include "dlls/engine/29_gplay.h"
#include "game/gamebits.h"
#include "sys/main.h"
#include "macros.h"

enum CheatInventoryItemType {
    TYPE_BOOL = 0,
    TYPE_INT = 1,
    TYPE_SECTION = 2
};

typedef struct {
    s16 bit;
    u8 dangerous;
    u8 type;
    const char *name;
} CheatInventoryItem;

static CheatInventoryItem krystalItems[] = {
    { BIT_CloudRunner_Grubs, FALSE, TYPE_INT, "Grubs" },
    { BIT_Krystal_Fireflies, FALSE, TYPE_INT, "Fireflies" },
    { BIT_Inventory_MoonSeeds, TRUE, TYPE_INT, "MoonSeeds" },
    { BIT_Krazoa_Translator, FALSE, TYPE_BOOL, "Krazoa Translator" },
    { BIT_Krystal_Foodbag_S, FALSE, TYPE_BOOL, "Foodbag (small)" },
    { BIT_Krystal_Foodbag_M, FALSE, TYPE_BOOL, "Foodbag (medium)" },
    { BIT_Krystal_Foodbag_L, FALSE, TYPE_BOOL, "Foodbag (large)" },
    { BIT_Krystal_Dino_Bag_S, FALSE, TYPE_BOOL, "Dinosaur Foodbag (small)" },
    { BIT_Krystal_Dino_Bag_M, FALSE, TYPE_BOOL, "Dinosaur Foodbag (medium)" },
    { BIT_Krystal_Dino_Bag_L, FALSE, TYPE_BOOL, "Dinosaur Foodbag (large)" },
    
    { -1, FALSE, TYPE_SECTION, "Warlock Mountain" },
    { BIT_Krystal_Warp_Crystal, FALSE, TYPE_BOOL, "WL Warp Krystal" },

    { -1, FALSE, TYPE_SECTION, "Cape Claw" },
    { BIT_Gold_Nugget_CC, FALSE, TYPE_BOOL, "CC Gold Nugget" },
    { BIT_CC_Cell_Door_Key, FALSE, TYPE_BOOL, "CC Cell Door Key" },
    { BIT_CC_Fire_Crystal, FALSE, TYPE_BOOL, "CC Fire Crystal" },
    { BIT_CC_Krazoa_Tablets, FALSE, TYPE_INT, "CC Krazoa Tablets" },

    { -1, FALSE, TYPE_SECTION, "CloudRunner Fortress" },
    { BIT_CRF_Prison_Key_1, FALSE, TYPE_BOOL, "CRF Prison Key 1" },
    { BIT_CRF_Prison_Key_2, FALSE, TYPE_BOOL, "CRF Prison Key 2" },
    { BIT_CRF_Power_Room_Key, FALSE, TYPE_BOOL, "CRF Power Room Key" },
    { BIT_CRF_Red_Power_Crystal, FALSE, TYPE_BOOL, "CRF Red Power Crystal" },
    { BIT_CRF_Green_Power_Crystal, FALSE, TYPE_BOOL, "CRF Green Power Crystal" },
    { BIT_CRF_Blue_Power_Crystal, FALSE, TYPE_BOOL, "CRF Blue Power Crystal" },
    { BIT_CRF_Treasure_Chest_Key, FALSE, TYPE_BOOL, "CRF Treasure Chest Key" },
    { BIT_CC_Engineers_Key, FALSE, TYPE_BOOL, "CRF Engineer's Key" },
    { BIT_SpellStone_CRF, TRUE, TYPE_BOOL, "CRF SpellStone (activated)" },
    
    { -1, FALSE, TYPE_SECTION, "Golden Plains" },
    { BIT_Gold_Nugget_GP, FALSE, TYPE_BOOL, "GP Gold Nugget" },

    { -1, FALSE, TYPE_SECTION, "LightFoot Village" },
    { BIT_Gold_Nugget_LFV, FALSE, TYPE_BOOL, "LFV Gold Nugget" },

    { -1, FALSE, TYPE_SECTION, "BlackWater Canyon" },
    { BIT_SpellStone_BWC, TRUE, TYPE_BOOL, "BWC SpellStone (activated)" },

    { -1, FALSE, TYPE_SECTION, "Krazoa Palace" },
    { BIT_SpellStone_KP, TRUE, TYPE_BOOL, "KP SpellStone (activated)" }
};

static CheatInventoryItem sabreItems[] = {
    { BIT_Inventory_Blue_Mushrooms, FALSE, TYPE_INT, "Blue Mushrooms" },
    { BIT_Inventory_Purple_Mushrooms, FALSE, TYPE_INT, "Purple Mushrooms" },
    { BIT_Inventory_White_Mushrooms, FALSE, TYPE_INT, "White Mushrooms" },
    { BIT_Horn_of_Truth, FALSE, TYPE_BOOL, "Horn of Truth" },
    { BIT_Sabre_Fireflies, FALSE, TYPE_INT, "Fireflies" },
    { BIT_Inventory_MoonSeeds, TRUE, TYPE_INT, "MoonSeeds" },
    { BIT_SP_Krazoa_Translator, FALSE, TYPE_BOOL, "Krazoa Translator" },
    { BIT_Sabre_Foodbag_S, FALSE, TYPE_BOOL, "Foodbag (small)" },
    { BIT_Sabre_Foodbag_M, FALSE, TYPE_BOOL, "Foodbag (medium)" },
    { BIT_Sabre_Foodbag_L, FALSE, TYPE_BOOL, "Foodbag (large)" },
    { BIT_Sabre_Dino_Bag_S, FALSE, TYPE_BOOL, "Dinosaur Foodbag (small)" },
    { BIT_Sabre_Dino_Bag_M, FALSE, TYPE_BOOL, "Dinosaur Foodbag (medium)" },
    { BIT_Sabre_Dino_Bag_L, FALSE, TYPE_BOOL, "Dinosaur Foodbag (large)" },

    { -1, FALSE, TYPE_SECTION, "SnowHorn Wastes" },
    { BIT_SW_Alpine_Roots, FALSE, TYPE_INT, "SW Alpine Roots" },
    
    { -1, FALSE, TYPE_SECTION, "DarkIce Mines" },
    { BIT_DIM_Mine_Key, FALSE, TYPE_BOOL, "DIM Mine Key" },
    { BIT_DIM_Gear_1, FALSE, TYPE_BOOL, "DIM Gear 1" },
    { BIT_DIM_Gear_2, FALSE, TYPE_BOOL, "DIM Gear 2" },
    { BIT_DIM_Gear_3, FALSE, TYPE_BOOL, "DIM Gear 3" },
    { BIT_DIM_Gear_4, FALSE, TYPE_BOOL, "DIM Gear 4" },
    { BIT_DIM_Alpine_Roots, FALSE, TYPE_INT, "DIM Alpine Roots" },
    { BIT_Belina_Te_Cell_Key, FALSE, TYPE_BOOL, "DIM Belina Te Cell Key" },
    { BIT_Tricky_Cell_Key, FALSE, TYPE_BOOL, "DIM Tricky Cell Key" },
    { BIT_DIM_Door_Key_1, FALSE, TYPE_BOOL, "DIM Door Key 1" },
    { BIT_DIM_Door_Key_2, FALSE, TYPE_BOOL, "DIM Door Key 2" },
    { BIT_SpellStone_DIM, TRUE, TYPE_BOOL, "DIM SpellStone" },
    { BIT_SpellStone_DIM_Activated, TRUE, TYPE_BOOL, "DIM SpellStone (activated)" },

    { -1, FALSE, TYPE_SECTION, "Diamond Bay" },
    { BIT_DB_PointBack_Egg, FALSE, TYPE_BOOL, "DB PointBack Egg" },
    { BIT_DB_Bay_Diamond, FALSE, TYPE_BOOL, "DB Bay Diamond" },

    { -1, FALSE, TYPE_SECTION, "Warlock Mountain" },
    { BIT_Sabre_Warp_Crystal, FALSE, TYPE_BOOL, "WL Warp Crystal" },

    { -1, FALSE, TYPE_SECTION, "Walled City" },
    { BIT_WC_Silver_Tooth, FALSE, TYPE_BOOL, "WC Silver Tooth" },
    { BIT_WC_Gold_Tooth, FALSE, TYPE_BOOL, "WC Gold Tooth" },
    { BIT_SpellStone_WC, TRUE, TYPE_BOOL, "WC SpellStone (activated)" },
    { BIT_WC_Sun_Stone, FALSE, TYPE_BOOL, "WC Sun Stone" },
    { BIT_WC_Moon_Stone, FALSE, TYPE_BOOL, "WC Moon Stone" },

    { -1, FALSE, TYPE_SECTION, "Dragon Rock" },
    { BIT_SpellStone_DR, TRUE, TYPE_BOOL, "DR SpellStone (activated)" }
};

static CheatInventoryItem magicSpells[] = {
    { BIT_Spell_Projectile, FALSE, TYPE_BOOL, "Projectile" },
    { BIT_Spell_Ice_Blast, FALSE, TYPE_BOOL, "Ice Blast" },
    { BIT_Spell_Grenade, FALSE, TYPE_BOOL, "Grenade" },
    { BIT_Spell_Illusion, FALSE, TYPE_BOOL, "Illusion" },
    { BIT_Spell_Forcefield, FALSE, TYPE_BOOL, "Forcefield" },
    { BIT_Spell_Portal, FALSE, TYPE_BOOL, "Portal" },
    { BIT_Spell_Mind_Read, FALSE, TYPE_BOOL, "Mind Read" }
};

static CheatInventoryItem trickyCommands[] = {
    { BIT_Tricky_Ball_Unlocked, FALSE, TYPE_BOOL, "Play" },
    { BIT_Tricky_Learned_Guard, FALSE, TYPE_BOOL, "Guard" },
    { BIT_Tricky_Learned_Distract, FALSE, TYPE_BOOL, "Distract" },
    { BIT_Tricky_Learned_Flame, FALSE, TYPE_BOOL, "Flame" }
};

static CheatInventoryItem foodbag[] = {
    { BIT_Dino_Egg_Count, FALSE, TYPE_INT, "Energy Eggs" },
    { BIT_Green_Apple_Count, FALSE, TYPE_INT, "Green Apples" },
    { BIT_Red_Apple_Count, FALSE, TYPE_INT, "Red Apples" },
    { BIT_Fish_Count, FALSE, TYPE_INT, "Fish" },
    { BIT_Smoked_Fish_Count, FALSE, TYPE_INT, "Smoked Fish" },
    { BIT_Green_Bean_Count, FALSE, TYPE_INT, "Green Beans" },
    { BIT_Red_Bean_Count, FALSE, TYPE_INT, "Red Beans" },
    { BIT_Blue_Bean_Count, FALSE, TYPE_INT, "Blue Beans" },

    { -1, FALSE, TYPE_SECTION, "Old" },
    { BIT_Moldy_Meat_Count, FALSE, TYPE_INT, "Moldy Energy Eggs" },
    { BIT_Brown_Apple_Count, FALSE, TYPE_INT, "Brown Apples" },
    { BIT_Brown_Bean_Count, FALSE, TYPE_INT, "Brown Beans" }
};

static CheatInventoryItem dinosaurFoodbag[] = {
    { BIT_Dino_Bag_Blue_Mushrooms, FALSE, TYPE_INT, "Blue Mushrooms" },
    { BIT_Dino_Bag_Red_Mushrooms, FALSE, TYPE_INT, "Red Mushrooms" },
    { BIT_Dino_Bag_Blue_Grubs, FALSE, TYPE_INT, "Blue Grubs" },
    { BIT_Dino_Bag_Red_Grubs, FALSE, TYPE_INT, "Red Grubs" },

    { -1, FALSE, TYPE_SECTION, "Old" },
    { BIT_Dino_Bag_Old_Mushrooms, FALSE, TYPE_INT, "Old Mushrooms" },
    { BIT_Dino_Bag_Old_Grubs, FALSE, TYPE_INT, "Old Grubs" }
};

static s32 infiniteHealth = FALSE;
static s32 infiniteMagic = FALSE;

static void bit_editor(CheatInventoryItem *item) {
    switch (item->type) {
        case TYPE_BOOL: {
            s32 checked = main_get_bits(item->bit);
            if (dbgui_checkbox(item->name, &checked)) {
                main_set_bits(item->bit, checked);
            }
            break;
        }
        case TYPE_INT: {
            s32 value = main_get_bits(item->bit);
            dbgui_set_next_item_width(100);
            if (dbgui_input_int(item->name, &value)) {
                main_set_bits(item->bit, value);
            }
            break;
        }
        case TYPE_SECTION:
            dbgui_new_line();
            dbgui_separator_text(item->name);
            break;
    }

    if (item->dangerous) {
        dbgui_set_item_tooltip("Danger: Modifying this item can affect story progression!");
    }
}

static void bag_items(void) {
    dbgui_text_wrapped("Note: Items that are no longer available due to story progression will not show up even if given.");

    if (dbgui_begin_tab_bar("tabs")) {
        if (dbgui_begin_tab_item("Krystal", NULL)) {
            if (dbgui_begin_child("scroll")) {
                for (s32 i = 0; i < (s32)ARRAYCOUNT(krystalItems); i++) {
                    bit_editor(&krystalItems[i]);
                }
            }
            dbgui_end_child();
            dbgui_end_tab_item();
        }
        if (dbgui_begin_tab_item("Sabre", NULL)) {
            if (dbgui_begin_child("scroll")) {
                for (s32 i = 0; i < (s32)ARRAYCOUNT(sabreItems); i++) {
                    bit_editor(&sabreItems[i]);
                }
            }
            dbgui_end_child();
            dbgui_end_tab_item();
        }
        dbgui_end_tab_bar();
    }
}

static void spell_book(void) {
    if (dbgui_begin_child("scroll")) {
        for (s32 i = 0; i < (s32)ARRAYCOUNT(magicSpells); i++) {
            bit_editor(&magicSpells[i]);
        }
    }
    dbgui_end_child();
}

static void foodbags(void) {
    if (dbgui_begin_tab_bar("tabs")) {
        if (dbgui_begin_tab_item("Foodbag", NULL)) {
            if (dbgui_begin_child("scroll")) {
                for (s32 i = 0; i < (s32)ARRAYCOUNT(foodbag); i++) {
                    bit_editor(&foodbag[i]);
                }
            }
            dbgui_end_child();
            dbgui_end_tab_item();
        }
        if (dbgui_begin_tab_item("Dinosaur Foodbag", NULL)) {
            if (dbgui_begin_child("scroll")) {
                for (s32 i = 0; i < (s32)ARRAYCOUNT(dinosaurFoodbag); i++) {
                    bit_editor(&dinosaurFoodbag[i]);
                }
            }
            dbgui_end_child();
            dbgui_end_tab_item();
        }
        dbgui_end_tab_bar();
    }
}

static void player_stats_editor(PlayerStats *stats) {
    dbgui_push_item_width(140);

    s32 hp = stats->health;
    if (dbgui_input_int("Health", &hp)) {
        if (hp < 0) hp = 0;
        stats->health = (u8)hp;
    }
    s32 hpMax = stats->healthMax;
    if (dbgui_input_int("Max Health", &hpMax)) {
        if (hpMax < 0) hpMax = 0;
        stats->healthMax = (u8)hpMax;
    }
    s32 magic = stats->magic;
    if (dbgui_input_int("Magic", &magic)) {
        if (magic < 0) magic = 0;
        stats->magic = (s16)magic;
    }
    s32 magicMax = stats->magicMax;
    if (dbgui_input_int("Max Magic", &magicMax)) {
        if (magicMax < 0) magicMax = 0;
        stats->magicMax = (s16)magicMax;
    }
    s32 scarabs = stats->scarabs;
    if (dbgui_input_int("Scarabs", &scarabs)) {
        if (scarabs < 0) scarabs = 0;
        stats->scarabs = (s16)scarabs;
    }
    s32 dusters = stats->dusters;
    if (dbgui_input_int("Dusters", &dusters)) {
        if (dusters < 0) dusters = 0;
        stats->dusters = (s8)dusters;
    }

    dbgui_pop_item_width();
}

static void player_stats(void) {
    dbgui_checkbox("Infinite Health", &infiniteHealth);
    dbgui_checkbox("Infinite Magic", &infiniteMagic);

    if (dbgui_begin_tab_bar("tabs")) {
        PlayerStats *statsArray = gDLL_29_Gplay->vtbl->get_state()->save.file.players;
        if (dbgui_begin_tab_item("Krystal", NULL)) {
            player_stats_editor(&statsArray[PLAYER_KRYSTAL]);
            dbgui_end_tab_item();
        }
        if (dbgui_begin_tab_item("Sabre", NULL)) {
            player_stats_editor(&statsArray[PLAYER_SABRE]);
            dbgui_end_tab_item();
        }
        dbgui_end_tab_bar();
    }
}

void dbgui_player_cheat_window(s32 *open) {
    if (dbgui_begin("Player Cheats", open)) {
        dbgui_text_wrapped("Warning: Cheats can cause sequence breaks!");
        if (dbgui_begin_tab_bar("tabs")) {
            if (dbgui_begin_tab_item("Stats", NULL)) {
                player_stats();
                dbgui_end_tab_item();
            }
            if (dbgui_begin_tab_item("Items", NULL)) {
                bag_items();
                dbgui_end_tab_item();
            }
            if (dbgui_begin_tab_item("Spell Book", NULL)) {
                spell_book();
                dbgui_end_tab_item();
            }
            if (dbgui_begin_tab_item("Foodbags", NULL)) {
                foodbags();
                dbgui_end_tab_item();
            }
            dbgui_end_tab_bar();
        }
    }
    dbgui_end();
}

static void sidekick_stats_editor(SidekickStats *stats, const char *foodName) {
    static DbgUiInputFloatOptions inputOptions = {
        .step = 0.5f,
        .stepFast = 1.0f,
        .format = "%.1f"
    };

    f32 blueFood = stats->blueFood / 2.0f;
    dbgui_set_next_item_width(140);
    if (dbgui_input_float_ext(recomp_sprintf_helper("Blue %s", foodName), &blueFood, &inputOptions)) {
        if (blueFood < 0) blueFood = 0;
        stats->blueFood = (u8)(blueFood * 2.0f);
    }

    f32 redFood = stats->redFood / 2.0f;
    dbgui_set_next_item_width(140);
    if (dbgui_input_float_ext(recomp_sprintf_helper("Red %s", foodName), &redFood, &inputOptions)) {
        if (redFood < 0) redFood = 0;
        stats->redFood = (u8)(redFood * 2.0f);
    }
}

static void sidekick_stats(void) {
    if (dbgui_begin_tab_bar("tabs")) {
        SidekickStats *statsArray = gDLL_29_Gplay->vtbl->get_state()->save.file.sidekicks;
        if (dbgui_begin_tab_item("Kyte", NULL)) {
            sidekick_stats_editor(&statsArray[PLAYER_KRYSTAL], "Grubs");
            dbgui_end_tab_item();
        }
        if (dbgui_begin_tab_item("Tricky", NULL)) {
            sidekick_stats_editor(&statsArray[PLAYER_SABRE], "Mushrooms");
            dbgui_end_tab_item();
        }
        dbgui_end_tab_bar();
    }
}

static void sidekick_commands(void) {
    if (dbgui_begin_tab_bar("tabs")) {
        if (dbgui_begin_tab_item("Kyte", NULL)) {
            dbgui_text("Kyte's commands are always unlocked!");
            dbgui_end_tab_item();
        }
        if (dbgui_begin_tab_item("Tricky", NULL)) {
            if (dbgui_begin_child("scroll")) {
                for (s32 i = 0; i < (s32)ARRAYCOUNT(trickyCommands); i++) {
                    bit_editor(&trickyCommands[i]);
                }
            }
            dbgui_end_child();
            dbgui_end_tab_item();
        }
        dbgui_end_tab_bar();
    }
}

void dbgui_sidekick_cheat_window(s32 *open) {
    if (dbgui_begin("Sidekick Cheats", open)) {
        dbgui_text_wrapped("Warning: Cheats can cause sequence breaks!");
        if (dbgui_begin_tab_bar("tabs")) {
            if (dbgui_begin_tab_item("Stats", NULL)) {
                sidekick_stats();
                dbgui_end_tab_item();
            }
            if (dbgui_begin_tab_item("Commands", NULL)) {
                sidekick_commands();
                dbgui_end_tab_item();
            }
            dbgui_end_tab_bar();
        }
    }
    dbgui_end();
}

void dbgui_cheat_game_tick(void) {
    PlayerStats *stats = gDLL_29_Gplay->vtbl->get_player_stats();

    if (infiniteHealth) {
        stats->health = stats->healthMax;
    }
    if (infiniteMagic) {
        stats->magic = stats->magicMax;
    }
}
