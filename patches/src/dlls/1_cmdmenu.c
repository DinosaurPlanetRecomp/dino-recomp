#include "patches.h"
#include "rt64_extended_gbi.h"
#include "patches/1_cmdmenu.h"

#include "PR/gbi.h"
#include "PR/ultratypes.h"
#include "PR/os.h"
#include "dlls/engine/1_cmdmenu.h"
#include "dlls/objects/210_player.h"
#include "dlls/objects/common/sidekick.h"
#include "game/gamebits.h"
#include "game/objects/object.h"
#include "game/objects/object_id.h"
#include "sys/gfx/texture.h"
#include "sys/gfx/textable.h"
#include "sys/camera.h"
#include "sys/objects.h"
#include "sys/rcp.h"
#include "sys/vi.h"
#include "sys/fonts.h"
#include "types.h"

#include "recomp/dlls/engine/1_cmdmenu_recomp.h"

#define MAX_LOADED_ITEMS 64
#define MAX_OPACITY 0xFF
#define MAX_OPACITY_F 255.0f

#define NO_GAMETEXT -1
#define NO_TEXTURE -1
#define NO_PAGE -1
#define NO_ITEM -1

#define SLOT_OCCUPIED 0
#define SLOT_PADDED 1

/* UI COORD MACROS (TO-DO: move to separate header?) */

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

//Change these to move the UI elements in the top-left of screen (character icon, health, magic)
#define UI_TOP_LEFT_X 0
#define UI_TOP_LEFT_Y 0

//Change these to move the UI elements in the top-right of screen (C-buttons, inventory scroll)
#define UI_TOP_RIGHT_X 0
#define UI_TOP_RIGHT_Y 0

//Change these to move the UI elements in the bottom-left of screen (item info pop-up, minimap)
#define UI_BOTTOM_LEFT_X 0
#define UI_BOTTOM_LEFT_Y 0

//Change these to move the UI elements in the bottom-right of screen (Scarab counter, active Spell/Command)
#define UI_BOTTOM_RIGHT_X 0
#define UI_BOTTOM_RIGHT_Y 0

/* UI TOP-LEFT */

//Character icon
#define CHARACTER_ICON_X 20
#define CHARACTER_ICON_Y 10

//Health
#define HEALTH_ICONS_X 60
#define HEALTH_ICONS_Y 20
#define APPLES_SPACING_X 10
#define APPLES_SPACING_Y 10
#define APPLES_ROW_1 7
#define APPLES_ROW_2 6
#define APPLES_ROW_1_OFFSET_X 0
#define APPLES_ROW_2_OFFSET_X 5
#define APPLES_ROW_3_OFFSET_X 0

#define APPLES_ROW_2_IDX (APPLES_ROW_1)
#define APPLES_ROW_3_IDX (APPLES_ROW_1 + APPLES_ROW_2)

//Magic
#define MAGIC_UNITS_PER_BAR 25
#define MAGIC_BARS_WIDTH 66
#define MAGIC_BARS_HEIGHT 14
#define MAGIC_BARS_X 23
#define MAGIC_BARS_Y 60
#define MAGIC_BARS_SPACING_Y 12
#define MAGIC_BARS_ZERO_POINT_X 13

/* UI TOP-RIGHT */

//Inventory page icon
#define PAGE_ICON_X 261
#define PAGE_ICON_Y 10

//Inventory item icons
#define MENU_ITEM_X 262
#define MENU_ITEM_Y 59
#define MENU_ITEM_WIDTH 32
#define MENU_ITEM_HEIGHT 24
#define MENU_ITEM_QUANTITY_OFFSET_X 14
#define MENU_ITEM_QUANTITY_OFFSET_Y 9

#define MENU_ITEM_QUANTITY_X (MENU_ITEM_X + MENU_ITEM_QUANTITY_OFFSET_X)

//Inventory scroll
#define MENU_HEIGHT_OPEN 72

#define MENU_SCROLL_WIDTH 40
#define MENU_SCROLL_HEIGHT 8

#define MENU_SCROLL_X (MENU_ITEM_X - (MENU_SCROLL_WIDTH - MENU_ITEM_WIDTH)/2)
#define MENU_SCROLL_TOP_Y (MENU_ITEM_Y - MENU_SCROLL_HEIGHT)
#define MENU_SCROLL_BOTTOM_Y (MENU_ITEM_Y)

#define MENU_SCROLL_CENTRE_Y (MENU_ITEM_Y + (MENU_ITEM_HEIGHT + MENU_ITEM_HEIGHT/2)) //Screen Y-coord in the middle of the inventory's 3 tiles

//Inventory item selection highlight
#define ITEM_HL_WIDTH 8
#define ITEM_HL_HEIGHT 6
#define ITEM_HL_MARGIN 4

#define ITEM_HL_X1 (MENU_ITEM_X + ITEM_HL_MARGIN)
#define ITEM_HL_Y1 (MENU_ITEM_Y + (MENU_HEIGHT_OPEN - MENU_ITEM_HEIGHT)/2 - 1)
#define ITEM_HL_X2 (MENU_ITEM_X + MENU_ITEM_WIDTH - ITEM_HL_WIDTH - ITEM_HL_MARGIN)
#define ITEM_HL_Y2 (ITEM_HL_Y1 + MENU_ITEM_HEIGHT - ITEM_HL_MARGIN)

//Sidekick meter
#define SIDEKICK_METER_X 250
#define SIDEKICK_METER_Y 21
#define SIDEKICK_METER_SPACING_X 9
#define SIDEKICK_METER_SPACING_Y 8
#define SIDEKICK_METER_ICONS_PER_COLUMN 4

//C buttons
#define C_BUTTONS_X 245
#define C_BUTTONS_Y 17

#define C_BUTTONS_LEFT_EMPTY_X (C_BUTTONS_X + 1)
#define C_BUTTONS_LEFT_EMPTY_Y (C_BUTTONS_Y + 9)

#define C_BUTTONS_DOWN_EMPTY_X (C_BUTTONS_X + 7)
#define C_BUTTONS_DOWN_EMPTY_Y (C_BUTTONS_Y + 26)

#define C_BUTTONS_RIGHT_EMPTY_X (C_BUTTONS_X + 29)
#define C_BUTTONS_RIGHT_EMPTY_Y (C_BUTTONS_Y + 17)

#define C_BUTTONS_LEFT_DOWN_BOOK_SIDEKICK_X (C_BUTTONS_X + 0)
#define C_BUTTONS_LEFT_DOWN_BOOK_SIDEKICK_Y (C_BUTTONS_Y + 0)

#define C_BUTTONS_RIGHT_BAG_X (C_BUTTONS_X + 30)
#define C_BUTTONS_RIGHT_BAG_Y (C_BUTTONS_Y + 8)

/* UI BOTTOM-LEFT */

//Item info pop-up
#define INFO_POPUP_X 20
#define INFO_POPUP_Y 175
#define INFO_POPUP_EDGE_WIDTH 16

#define INFO_POPUP_L_X (INFO_POPUP_X + 0)
#define INFO_POPUP_M_X (INFO_POPUP_L_X + INFO_POPUP_EDGE_WIDTH)
#define INFO_POPUP_R_X (INFO_POPUP_M_X + MENU_ITEM_WIDTH)
#define INFO_POPUP_SHADOW_X (INFO_POPUP_X + 2)
#define INFO_POPUP_SHADOW_Y (INFO_POPUP_Y + 1)
#define INFO_POPUP_QUANTITY_X (INFO_POPUP_M_X + 16)
#define INFO_POPUP_QUANTITY_Y (INFO_POPUP_Y + 16)

/* UI BOTTOM-RIGHT */

//Scarabs counter
#define SCARABS_ICON_X 252
#define SCARABS_ICON_Y 198
#define SCARABS_ICON_WIDTH 16
#define SCARABS_ICON_HEIGTH 16
#define SCARABS_NUMBER_X (SCARABS_ICON_X + 18)
#define SCARABS_NUMBER_Y (SCARABS_ICON_Y + 4)

//Active Spell
#define ACTIVE_SPELL_X 253
#define ACTIVE_SPELL_Y 169
#define ACTIVE_SPELL_ICON_OFFSET_X 9
#define ACTIVE_SPELL_ICON_OFFSET_Y 11

#define ACTIVE_SPELL_ICON_X (ACTIVE_SPELL_X + ACTIVE_SPELL_ICON_OFFSET_X)
#define ACTIVE_SPELL_ICON_Y (ACTIVE_SPELL_Y + ACTIVE_SPELL_ICON_OFFSET_Y)

//Active Sidekick Command
#define ACTIVE_SIDECOMMAND_X 253
#define ACTIVE_SIDECOMMAND_Y 121
#define ACTIVE_SIDECOMMAND_ICON_OFFSET_X 9
#define ACTIVE_SIDECOMMAND_ICON_OFFSET_Y 11

#define ACTIVE_SIDECOMMAND_ICON_X (ACTIVE_SIDECOMMAND_X + ACTIVE_SIDECOMMAND_ICON_OFFSET_X)
#define ACTIVE_SIDECOMMAND_ICON_Y (ACTIVE_SIDECOMMAND_Y + ACTIVE_SIDECOMMAND_ICON_OFFSET_Y)

/* CENTRED UI */

//Info scroll
#define INFO_SCROLL_WIDTH 120
#define INFO_SCROLL_HEIGHT 50 //When open
#define INFO_SCROLL_TEXT_Y 3 //Top margin for text printed inside the tutorial box
#define INFO_SCROLL_LINE_HEIGHT 16 //Text lines' vertical spacing
#define INFO_SCROLL_X (SCREEN_WIDTH/2)
#define INFO_SCROLL_Y 30
#define INFO_SCROLL_Y_INITIAL (INFO_SCROLL_Y - 10)
#define INFO_SCROLL_OPACITY_MAX 160
#define INFO_SCROLL_OPACITY_SPEED 32

//Info scroll texture dimensions
#define INFO_SCROLL_PAGE_EDGE_WIDTH 16
#define INFO_SCROLL_PAGE_SHADOW_HEIGHT 8
#define INFO_SCROLL_ROLL_HEIGHT 16
#define INFO_SCROLL_ROLL_TOP_Y_OFFSET 11
#define INFO_SCROLL_ROLL_BOTTOM_Y_OFFSET 4 //NOTE: causes 1px gap, may be intentional as dark shadow
#define INFO_SCROLL_HANDLE_WIDTH 16
#define INFO_SCROLL_HANDLE_HEIGHT 16

//Tutorial textbox
#define TUTORIAL_BOX_WIDTH 240
#define TUTORIAL_BOX_HEIGHT 80
#define TUTORIAL_BOX_TEXT_Y 3 //Top margin for text printed inside the tutorial box
#define TUTORIAL_BOX_LINE_HEIGHT 16 //Text lines' vertical spacing
#define TUTORIAL_BOX_X (SCREEN_WIDTH/2)
#define TUTORIAL_BOX_Y 20
#define TUTORIAL_BOX_OPACITY_MAX 160
#define TUTORIAL_BOX_OPACITY_SPEED 8

//Tutorial textbox texture dimensions
#define TUTORIAL_BOX_PAGE_EDGE_WIDTH 16
#define TUTORIAL_BOX_PAGE_SHADOW_HEIGHT 8
#define TUTORIAL_BOX_ROLL_HEIGHT 16
#define TUTORIAL_BOX_ROLL_TOP_Y_OFFSET 11
#define TUTORIAL_BOX_ROLL_BOTTOM_Y_OFFSET 4 //NOTE: causes 1px gap, may be intentional as dark shadow
#define TUTORIAL_BOX_HANDLE_WIDTH 16
#define TUTORIAL_BOX_HANDLE_HEIGHT 16

#define TUTORIAL_BOX_A_BUTTON_WIDTH 24
#define TUTORIAL_BOX_A_BUTTON_HEIGHT 24
#define TUTORIAL_BOX_A_BUTTON_OFFSET_X 8
#define TUTORIAL_BOX_A_BUTTON_OFFSET_Y 0

//Energy bar
#define ENERGY_BAR_X (SCREEN_WIDTH / 2)
#define ENERGY_BAR_Y (SCREEN_HEIGHT - 10)

//Aiming reticle
#define AIMING_RETICLE_WIDTH 32
#define AIMING_RETICLE_HEIGHT 32
#define AIMING_RETICLE_OPACITY 150

enum CmdMenuTextures {
    CMDMENU_TEX_00_Scroll_BG = 0,
    CMDMENU_TEX_01_Scroll_Bottom = 1,
    CMDMENU_TEX_02_Scroll_Top = 2,
    CMDMENU_TEX_03_InfoScroll_Roll_End = 3,
    CMDMENU_TEX_04_InfoScroll_Roll = 4,
    CMDMENU_TEX_05_InfoScroll_Side = 5,
    CMDMENU_TEX_06_InfoScroll_BG = 6,
    CMDMENU_TEX_07_InfoScroll_SelfShadow = 7,
    CMDMENU_TEX_08_Apple_0_Pct = 8,
    CMDMENU_TEX_09_Apple_25_Pct = 9,
    CMDMENU_TEX_10_Apple_50_Pct = 10,
    CMDMENU_TEX_11_Apple_75_Pct = 11,
    CMDMENU_TEX_12_Mushroom_Blue_Full = 12,
    CMDMENU_TEX_13_Grub_Blue_Full = 13,
    CMDMENU_TEX_14_Unk_Circle_Glow = 14,
    CMDMENU_TEX_15_Unk_Circle_Blue = 15,
    CMDMENU_TEX_16_Grub_Blue_Half = 16,
    CMDMENU_TEX_17_Apple_100_Pct = 17,
    CMDMENU_TEX_18_Scarab = 18,
    CMDMENU_TEX_19_Scarab_Flutter_Frame1 = 19,
    CMDMENU_TEX_20_Scarab_Flutter_Frame2 = 20,
    CMDMENU_TEX_21_Scarab_Flutter_Frame3 = 21,
    CMDMENU_TEX_22_Scarab_Spin_Frame1 = 22,
    CMDMENU_TEX_23_Scarab_Spin_Frame2 = 23,
    CMDMENU_TEX_24_Scarab_Spin_Frame3 = 24,
    CMDMENU_TEX_25_Scarab_Spin_Frame4 = 25,
    CMDMENU_TEX_26_Scarab_Spin_Frame5 = 26,
    CMDMENU_TEX_27_Scarab_Spin_Frame6 = 27,
    CMDMENU_TEX_28_Scarab_Spin_Frame7 = 28,
    CMDMENU_TEX_29_Page_Torn_Left = 29,
    CMDMENU_TEX_30_Page_Torn_Right = 30,
    CMDMENU_TEX_31_Highlight_Corner_Top_Left = 31,
    CMDMENU_TEX_32_Highlight_Corner_Top_Right = 32,
    CMDMENU_TEX_33_Highlight_Corner_Bottom_Left = 33,
    CMDMENU_TEX_34_Highlight_Corner_Bottom_Right = 34,
    CMDMENU_TEX_35_MagicBar_Empty = 35,
    CMDMENU_TEX_36_MagicBar_Full = 36,
    CMDMENU_TEX_37_C_Down = 37,
    CMDMENU_TEX_38_LeftDownButtons_SpellBook_With_Kyte = 38,
    CMDMENU_TEX_39_C_Left = 39,
    CMDMENU_TEX_40_Sabre = 40,
    CMDMENU_TEX_41_C_Right = 41,
    CMDMENU_TEX_42_Tricky = 42,
    CMDMENU_TEX_43_LeftDownButtons_SpellBook_With_Tricky = 43,
    CMDMENU_TEX_44_Mushroom_Empty = 44,
    CMDMENU_TEX_45_Mushroom_Red_Full = 45,
    CMDMENU_TEX_46_Mushroom_Red_Half = 46,
    CMDMENU_TEX_47_RightButton_With_Bag = 47,
    CMDMENU_TEX_48_LeftDownButtons_SpellBook_NoSidekick = 48,
    CMDMENU_TEX_49_MagicBook = 49,
    CMDMENU_TEX_50_Bag = 50,
    CMDMENU_TEX_51_Mushroom_Blue_Half = 51,
    CMDMENU_TEX_52_Page_Torn_Shadow = 52,
    CMDMENU_TEX_53_Krystal = 53,
    CMDMENU_TEX_54_Kyte = 54,
    CMDMENU_TEX_55_Grub_Empty = 55,
    CMDMENU_TEX_56_Grub_Red_Full = 56,
    CMDMENU_TEX_57_Grub_Red_Half = 57
};

/*0x0*/ extern f32 sOpacityHealth;  //Opacity of player health UI
/*0x4*/ extern f32 sOpacityScarabs; //Opacity of Scarab counter UI
/*0x8*/ extern f32 sOpacityMagic;   //Opacity of player magic bar UI
/*0x10*/ extern CmdmenuPlayerSidekickData sStats;
/*0x60*/ extern CmdmenuPlayerSidekickDataChangeTimers sStatsChangeTimers;
/*0x89*/ extern u8 sAnimFrameScarab;        //Frame offset for the Scarab (an ID offset in practice, since Scarab's animation frames are stored as separate textures)
/*0x98*/ extern Texture* sMenuItemTextures[MAX_LOADED_ITEMS];           //Inventory icon texture pointers for the current menu page's loaded items
/*0x8A*/ extern u8 sAnimScarabFlutterTimer; //Plays Scarab flutter animation during last ticks of countdown
/*0x8B*/ extern u8 sAnimScarabSpin;         //Plays Scarab spin animation when nonzero (used as frame offset)
/*0x518*/ extern u8 sMenuItemVisibilities[MAX_LOADED_ITEMS];            //Visibility Booleans for each of the current menu page's loaded items (Spells are the only kind of inventory item that still load while their gamebitHidden is set, however they're drawn as an empty tile when hidden)
/*0x558*/ extern u8 sMenuItemQuantities[MAX_LOADED_ITEMS];              //Item quantities for each of the current menu page's loaded items
/*0x598*/ extern Texture* sActiveSpellIcon;             //Icon in bottom-right of screen, showing the Spell currently in use
/*0x59C*/ extern Texture* sActiveSpellRing;             //Icon in bottom-right of screen, circling the Spell currently in use
/*0x5A4*/ extern s16 sPrevActiveSpellGamebit;           //The gamebitID of the mostly recently-used Spell (used to check if the active Spell changed)
/*0x5A8*/ extern Texture* sActiveSidekickCommandIcon;   //Icon in bottom-right of screen, showing the Sidekick Command currently in use
/*0x5AC*/ extern Texture* sActiveSidekickCommandRing;   //Icon in bottom-right of screen, circling the Sidekick Command currently in use
/*0x5B0*/ extern s16 sPrevSidekickCommandIndex;         //The index of the mostly recently-used Sidekick Command (used to check if the active Sidekick Command changed)
/*0x5B8*/ extern s32 sCrosshairAnimRenderFlags;
/*0x5C0*/ extern s32 sCrosshairAnimProgress;
/*0x6B0*/ extern Texture* sInventoryStackNumbersTex;
/*0x6B8*/ extern TextureTile sTextureTiles[58][2];
/*0xC28*/ extern s16 sInventoryUnrollY;  //How far the inventory scroll has opened (0 when fully closed)
/*0xC2A*/ extern s16 sInfoScrollUnrollY; //How far the R-button info scroll has opened (0 when fully closed)
/*0xC34*/ extern Texture* sCrosshairTex;
/*0xC3E*/ extern s8 sInventoryPageID;     //The pageID currently open (see `CmdMenuPages`)
/*0xC40*/ extern s16 sMenuSelectedItemIdx; //Display index of the item currently selected in the menu page 
/*0xC44*/ extern s32 sDisplayedItemCount;  //The number of items displayed on the current page (while drawing the inventory icons, this number is updated to be at least the number of slots in the tile strip)
/*0xC58*/ extern s32 sJoyHeldButtons;               //Joypad button bitfield
/*0xC60*/ extern TextureTile sTempIcon[2];
/*0xC88*/ extern CmdmenuInfoPopup sInfoPopup;   //Item info pop-up that appears after collecting certain items (e.g. Kyte's grubs)

/*0x0*/ extern s8 dInventoryShow;
/*0x4*/ extern s8 sInventoryScrollOffset; //Y offset for the scroll's inner strip, used to smoothly animate moving between inventory items
/*0xC*/ extern s16 dInventoryUnrollMax; //The height of the inventory scroll when fully open
/*0x10*/ extern s16 dInventoryOpacity;
/*0x14*/ extern s16 dOpacitySidekickMeter;
/*0x68*/ extern Texture* dInventoryPageIcon; //Icon in the top-right corner of screen: Bag/SpellBook/Kyte/Tricky (also used for C-right button)
/*0xAC*/ extern s16 dCommandTextableIDs[6];
/*0x9D8*/ extern s16 dTextableIDs[58];

extern void cmdmenu_info_draw(Gfx** gdl, CmdmenuInfoPopup* box);
extern s16 cmdmenu_get_spell_textable(s32 spellGamebit);
extern void cmdmenu_draw_energy_bar(Gfx** gdl);
extern void cmdmenu_gfx_set_scroll_scissor(Gfx** gdl);
extern void cmdmenu_gfx_set_screen_scissor(Gfx** gdl);
extern void cmdmenu_draw_player_stats(Gfx** gdl, Mtx** mtxs, Vertex** vtxs);
extern void cmdmenu_draw_c_buttons_and_sidekick_meter(Gfx** gdl, Mtx** mtxs, Vertex** vtxs);
extern void cmdmenu_draw_info_scroll(Gfx** gdl, Mtx** mtxs, Vertex** vtxs);
extern void cmdmenu_draw_tutorial_textbox(Gfx** gdl, Mtx** mtxs, Vertex** vtxs);
extern void cmdmenu_draw_main(Gfx** gdl, Mtx** mtxs, Vertex** vtxs);

_Bool recomp_cmdmenu_is_r_held(void) {
    return (sJoyHeldButtons & R_TRIG) ? 1 : 0;
}

RECOMP_PATCH void cmdmenu_print(Gfx** gdl, Mtx** mtxs, Vertex** vtxs) {
    Object* player;
    s32 viSize;
    s32 screenX;
    s32 screenY;

    player = get_player();
    if (player == NULL) {
        return;
    }

    // @recomp: Fullscreen scissors
    gEXSetScissorAlign((*gdl)++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, -SCREEN_WIDTH, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    //Draw Spell reticle when aiming (@bug: x coord not adjusted in widescreen)
    if (((DLL_210_Player*)player->dll)->vtbl->func77(player, &screenX, &screenY)) {
        tex_animate(sCrosshairTex, &sCrosshairAnimRenderFlags, &sCrosshairAnimProgress);
        rcp_screen_full_write(
            gdl, 
            sCrosshairTex, 
            screenX - (AIMING_RETICLE_WIDTH/2), 
            screenY - (AIMING_RETICLE_HEIGHT/2), 
            0, 
            sCrosshairAnimProgress >> 8, 
            AIMING_RETICLE_OPACITY, 
            SCREEN_WRITE_TRANSLUCENT
        );
    }

    cmdmenu_draw_player_stats(gdl, mtxs, vtxs);

    viSize = vi_get_current_size();
    gDPSetScissor((*gdl)++, G_SC_NON_INTERLACE, 
        0, 
        0, 
        GET_VIDEO_WIDTH(viSize), 
        GET_VIDEO_HEIGHT(viSize));

    // @recomp: Align C buttons/sidekick meter to right
    gEXSetViewportAlign((*gdl)++, G_EX_ORIGIN_RIGHT, -SCREEN_WIDTH * 4, 0);
    gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -SCREEN_WIDTH * 4, 0, -SCREEN_WIDTH * 4, 0);

    cmdmenu_draw_c_buttons_and_sidekick_meter(gdl, mtxs, vtxs);

    // @recomp: Reset alignment
    gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXSetViewportAlign((*gdl)++, G_EX_ORIGIN_NONE, 0, 0);

    cmdmenu_draw_info_scroll(gdl, mtxs, vtxs);
    cmdmenu_draw_tutorial_textbox(gdl, mtxs, vtxs);
    cmdmenu_draw_main(gdl, mtxs, vtxs);
    camera_apply_scissor(gdl);

    // @recomp: Reset scissor align
    gEXSetScissorAlign((*gdl)++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

RECOMP_PATCH void cmdmenu_draw_player_stats(Gfx** gdl, Mtx** mtxs, Vertex** vtxs) {
    f32 goalOpacity;
    u32 pad;
    u8 i;
    u8 statsOpacity;
    s8 offsetX;
    s8 offsetY;
    s32 temp;
    Gfx* dl;
    Object* player = get_player();
    u8 texIdx;
    char playerScarabCountText[4] = "   ";

    dl = *gdl;
    temp = vi_get_current_size();
    
    gDPSetScissor(dl++, G_SC_NON_INTERLACE, 0, 0, (u16)GET_VIDEO_WIDTH(temp) - 1, SCREEN_HEIGHT - 1);

    // @recomp: Align stats to left
    gEXSetViewportAlign(dl++, G_EX_ORIGIN_LEFT, 0, 0);
    gEXSetRectAlign(dl++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, 0, 0, 0, 0);
    
    //Draw player health
    {
        if ((sStatsChangeTimers.playerHealth >= 0.0f) || 
            (sStatsChangeTimers.playerHealthMax >= 0.0f) || 
            (sStatsChangeTimers.unk14 >= 0.0f) //Shows health when pressing R
        ) {
            goalOpacity = MAX_OPACITY_F;
        } else {
            goalOpacity = 0.0f;
        }

        if (sOpacityHealth < goalOpacity) {
            sOpacityHealth += 8.5f * gUpdateRateF;
            if (sOpacityHealth > MAX_OPACITY_F) {
                sOpacityHealth = MAX_OPACITY_F;
            }
        } else if (goalOpacity < sOpacityHealth) {
            sOpacityHealth -= 8.5f * gUpdateRateF;
            if (sOpacityHealth < 0.0f) {
                sOpacityHealth = 0.0f;
            }
        }
        
        statsOpacity = sOpacityHealth;
        if (statsOpacity) {
            for (i = 0; i < (sStats.playerHealthMax >> 2); i++) {
                s32 appleCount;

                //Get apple coords (3 rows)
                if (i >= APPLES_ROW_3_IDX) {
                    //Row 3
                    offsetX = (i * APPLES_SPACING_X) + (APPLES_ROW_3_OFFSET_X - (APPLES_ROW_3_IDX * APPLES_SPACING_X));
                    offsetY = 2 * APPLES_SPACING_Y;
                } else if (i >= APPLES_ROW_2_IDX) {
                    //Row 2
                    offsetX = (i * APPLES_SPACING_X) + (APPLES_ROW_2_OFFSET_X - (APPLES_ROW_2_IDX * APPLES_SPACING_Y));
                    offsetY = APPLES_SPACING_Y;
                } else {
                    //Row 1
                    offsetX = (i * APPLES_SPACING_X) + (APPLES_ROW_1_OFFSET_X);
                    offsetY = 0;
                }

                //Pick which texture to use for this apple
                appleCount = sStats.playerHealth >> 2;
                if (i < appleCount) {
                    //Full apple
                    texIdx = CMDMENU_TEX_17_Apple_100_Pct;
                } else if (appleCount < i) {
                    //Empty apple
                    texIdx = CMDMENU_TEX_08_Apple_0_Pct;
                } else {
                    //Portion of an apple
                    texIdx = CMDMENU_TEX_08_Apple_0_Pct + (sStats.playerHealth & 3);
                }
                
                rcp_tile_write(
                    &dl,
                    sTextureTiles[texIdx], 
                    HEALTH_ICONS_X + offsetX, 
                    HEALTH_ICONS_Y + offsetY,
                    0xFF,0xFF, 0xFF, statsOpacity
                );
            }
        }
    }

    //Draw player magic
    {
        //Get opacity goal
        if ((sStatsChangeTimers.playerMagic >= 0.0f) || 
            (sStatsChangeTimers.unk14 >= 0.0f) || 
            (((DLL_210_Player*)player->dll)->vtbl->func50(player) != -1)
        ) {
            goalOpacity = MAX_OPACITY_F;
        } else {
            goalOpacity = 0.0f;
        }

        //Animate magic bar's opacity towards goal opacity
        if (sOpacityMagic < goalOpacity) {
            sOpacityMagic += 8.5f * gUpdateRateF;
            if (sOpacityMagic > MAX_OPACITY_F) {
                sOpacityMagic = MAX_OPACITY_F;
            }
        } else if (goalOpacity < sOpacityMagic) {
            sOpacityMagic -= 8.5f * gUpdateRateF;
            if (sOpacityMagic < 0.0f) {
                sOpacityMagic = 0.0f;
            }
        }

        //Draw magic bar(s)
        statsOpacity = sOpacityMagic;
        if (statsOpacity) {
            //Draw a magic bar for every 25 units of the player's max magic (just 1 bar initially)
            for (i = 0; i < (sStats.playerMagicMax / MAGIC_UNITS_PER_BAR); i++) {
                if (i < (sStats.playerMagic / MAGIC_UNITS_PER_BAR)) {
                    //Full bar
                    temp = MAGIC_BARS_WIDTH;
                } else if ((sStats.playerMagic / MAGIC_UNITS_PER_BAR) < i) {
                    //Empty bar
                    temp = 0;
                } else {
                    //Partial bar
                    temp = MAGIC_BARS_ZERO_POINT_X + ((sStats.playerMagic % MAGIC_UNITS_PER_BAR) * 2);
                }

                //Draw the filled part of the bar
                rcp_tile_write_x(
                    &dl,
                    sTextureTiles[CMDMENU_TEX_36_MagicBar_Full],
                    MAGIC_BARS_X,
                    MAGIC_BARS_Y + (i * MAGIC_BARS_SPACING_Y),
                    temp,
                    MAGIC_BARS_HEIGHT,
                    0, 0,
                    1.0f, 1.0f,
                    statsOpacity | ~0xFF,
                    TILE_WRITE_TRANSLUCENT | TILE_WRITE_POINT_FILT
                );

                //Draw the empty part of the bar
                rcp_tile_write_x(
                    &dl, sTextureTiles[CMDMENU_TEX_35_MagicBar_Empty],
                    MAGIC_BARS_X + temp,
                    MAGIC_BARS_Y + (i * MAGIC_BARS_SPACING_Y),
                    (MAGIC_BARS_WIDTH - temp), 
                    MAGIC_BARS_HEIGHT,
                    temp << 5, 0, 
                    1.0f, 1.0f, 
                    statsOpacity | ~0xFF, 
                    TILE_WRITE_TRANSLUCENT | TILE_WRITE_POINT_FILT
                );
            }
        }
    }

    //Draw character icon
    {
        statsOpacity = ((u8)sOpacityHealth < (u8)sOpacityMagic) ? (u8)sOpacityMagic : (u8)sOpacityHealth;
        if (statsOpacity) {
            if (player->id == OBJ_Krystal) {
                offsetX = 0;
                offsetY = 0;
                texIdx = CMDMENU_TEX_53_Krystal;
            } else {
                offsetX = 2;
                offsetY = -1;
                texIdx = CMDMENU_TEX_40_Sabre;
            }

            dInventoryPageIcon = tex_load_deferred(dTextableIDs[texIdx]);

            rcp_screen_full_write(
                &dl,
                dInventoryPageIcon,
                CHARACTER_ICON_X + offsetX,
                CHARACTER_ICON_Y + offsetY,
                0,
                0,
                statsOpacity,
                SCREEN_WRITE_TRANSLUCENT
            );

            tex_free(dInventoryPageIcon);
        }
    }

    // @recomp: Align scarab counter to right
    gEXSetViewportAlign(dl++, G_EX_ORIGIN_RIGHT, -SCREEN_WIDTH * 4, 0);
    gEXSetRectAlign(dl++, G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -SCREEN_WIDTH * 4, 0, -SCREEN_WIDTH * 4, 0);

    //Draw Scarab counter
    {
        if ((sStatsChangeTimers.playerScarabCount >= 0.0f) || (sStatsChangeTimers.unk14 >= 0.0f)) {
            goalOpacity = MAX_OPACITY_F;
        } else {
            goalOpacity = 0.0f;
        }

        if (sOpacityScarabs < goalOpacity) {
            sOpacityScarabs += 8.5f * gUpdateRateF;
            if (sOpacityScarabs > MAX_OPACITY_F) {
                sOpacityScarabs = MAX_OPACITY_F;
            }
        } else if (goalOpacity < sOpacityScarabs) {
            sOpacityScarabs -= 8.5f * gUpdateRateF;
            if (sOpacityScarabs < 0.0f) {
                sOpacityScarabs = 0.0f;
            }
        }

        statsOpacity = sOpacityScarabs;
        if (statsOpacity && main_get_bits(BIT_UI_Scarab_Counter_Enabled)) {
            sAnimFrameScarab = 0;
            if (statsOpacity == MAX_OPACITY) {
                if ((i = sAnimScarabSpin)) { //@fake assignment of i
                    //Animate Scarab spinning (upon collecting Scarabs)
                    sAnimFrameScarab = 11 - sAnimScarabSpin;
                    sAnimScarabSpin--; //@framerate-dependent
                    if (sAnimScarabSpin == 0) {
                        sAnimScarabFlutterTimer = 80;
                    }
                } else {
                    //Animate Scarab fluttering wings
                    if (sAnimScarabFlutterTimer) {
                        sAnimScarabFlutterTimer--; //@framerate-dependent

                        //Change the Scarab's frame during last few ticks of the timer 
                        if (sAnimScarabFlutterTimer < 6) {
                            sAnimFrameScarab = 3 - (sAnimScarabFlutterTimer >> 1);
                        }
                    } else {
                        sAnimScarabFlutterTimer = rand_next(20, 255);
                    }
                }
            }

            rcp_tile_write_x(
                &dl, 
                sTextureTiles[CMDMENU_TEX_18_Scarab + sAnimFrameScarab], 
                SCARABS_ICON_X, 
                SCARABS_ICON_Y, 
                SCARABS_ICON_WIDTH, 
                SCARABS_ICON_HEIGTH, 
                0, 0, 
                1.0f, 1.0f, 
                statsOpacity | ~0xFF, 
                TILE_WRITE_TRANSLUCENT | TILE_WRITE_POINT_FILT
            );

            sprintf(playerScarabCountText, "%d", (int)sStats.playerScarabCount);
            font_window_set_coords(3, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            font_window_use_font(3, FONT_DINO_SUBTITLE_FONT_1);
            font_window_set_bg_colour(3, 0, 0, 0, 0);
            font_window_flush_strings(3);
            font_window_set_text_colour(3, 0xFF, 0xFF, 0xFF, 0xFF, statsOpacity);
            font_window_add_string_xy(3, SCARABS_NUMBER_X, SCARABS_NUMBER_Y, playerScarabCountText, 1, ALIGN_TOP_LEFT);
            font_window_set_text_colour(3, 0x14, 0x14, 0x14, 0xFF, 0xFF);
            font_window_use_font(3, FONT_DINO_SUBTITLE_FONT_1);
            font_window_draw(&dl, mtxs, vtxs, 3);
        }
    }

    // @recomp: Reset alignment
    gEXSetRectAlign(dl++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXSetViewportAlign(dl++, G_EX_ORIGIN_NONE, 0, 0);

    *gdl = dl;
}

RECOMP_PATCH void cmdmenu_draw_main(Gfx** gdl, Mtx** mtxs, Vertex** vtxs) {
    s16 commandTexTableID;
    Object* player;
    s32 activeSpellGamebit;
    s32 stripY;
    s32 spellTexTableID;
    s32 numSlotsAboveSelected;
    s8 slot[MAX_LOADED_ITEMS];
    s32 itemIdx;
    s32 i;
    s32 sideCommandIndex;
    s32 tileCount;
    u8 iconOpacity;
    u8 pageIcon;
    s8 offsetX;
    s8 offsetY;
    Object* sidekick;

    player = get_player();
    offsetX = 0;
    offsetY = 0;
    sidekick = get_sidekick();

    // @recomp: Align item popup to left
    gEXSetViewportAlign((*gdl)++, G_EX_ORIGIN_LEFT, 0, 0);
    gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, 0, 0, 0, 0);

    //Call the item info pop-up's draw
    cmdmenu_info_draw(gdl, &sInfoPopup);

    // @recomp: Align active spell/active sidekick command to right
    gEXSetViewportAlign((*gdl)++, G_EX_ORIGIN_RIGHT, -SCREEN_WIDTH * 4, 0);
    gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -SCREEN_WIDTH * 4, 0, -SCREEN_WIDTH * 4, 0);

    //Draw active spell icon
    {
        activeSpellGamebit = ((DLL_210_Player*)player->dll)->vtbl->func50(player);

        //Clear the icon's data before any change
        if ((sActiveSpellIcon != NULL) && (activeSpellGamebit != sPrevActiveSpellGamebit)) {
            tex_free(sActiveSpellRing);
            tex_free(sActiveSpellIcon);
            sPrevActiveSpellGamebit = NO_GAMEBIT;
            sActiveSpellIcon = NULL;
        }

        //Load the icon's textures when needed
        if ((sActiveSpellIcon == NULL) && (activeSpellGamebit != NO_GAMEBIT)) {
            spellTexTableID = cmdmenu_get_spell_textable(activeSpellGamebit);
            if (spellTexTableID != NO_TEXTURE) {
                sActiveSpellIcon = tex_load_deferred(spellTexTableID);
                sActiveSpellRing = tex_load_deferred(TEXTABLE_574_CMDMENU_Active_Spell_Ring);
            }
        }

        sPrevActiveSpellGamebit = activeSpellGamebit;

        if (sActiveSpellIcon != NULL) {
            rcp_screen_full_write(gdl, sActiveSpellRing, ACTIVE_SPELL_X,      ACTIVE_SPELL_Y,      0, 0, 0xFF, SCREEN_WRITE_TRANSLUCENT);
            rcp_screen_full_write(gdl, sActiveSpellIcon, ACTIVE_SPELL_ICON_X, ACTIVE_SPELL_ICON_Y, 0, 0, 0xFF, SCREEN_WRITE_TRANSLUCENT);
        }
    }

    //Draw active sidekick command icon
    if (sidekick != NULL) {
        // @bug: sideCommandIndex is undefined if this sidekick func returns 0
#ifndef AVOID_UB
        ((DLL_ISidekick*)sidekick->dll)->vtbl->func26(sidekick, &sideCommandIndex);
#else
        if (!((DLL_ISidekick*)sidekick->dll)->vtbl->func26(sidekick, &sideCommandIndex)) {
            sideCommandIndex = sPrevSidekickCommandIndex;
        }
#endif

        //Clear the icon's data before any change
        if ((sActiveSidekickCommandIcon != NULL) && (sideCommandIndex != sPrevSidekickCommandIndex)) {
            tex_free(sActiveSidekickCommandRing);
            tex_free(sActiveSidekickCommandIcon);
            sPrevSidekickCommandIndex = NO_SIDEKICK_COMMAND;
            sActiveSidekickCommandIcon = NULL;
        }

        //Load the icon's textures when needed
        if (sActiveSidekickCommandIcon == NULL && sideCommandIndex > 0) {
            commandTexTableID = dCommandTextableIDs[sideCommandIndex];
            if (commandTexTableID != NO_TEXTURE) {
                sActiveSidekickCommandIcon = tex_load_deferred(commandTexTableID);
                sActiveSidekickCommandRing = tex_load_deferred(TEXTABLE_584_CMDMENU_Active_Sidekick_Command_Ring);
            }
        }

        sPrevSidekickCommandIndex = sideCommandIndex;

        if (sActiveSidekickCommandIcon != NULL) {
            rcp_screen_full_write(gdl, sActiveSidekickCommandRing, ACTIVE_SIDECOMMAND_X,      ACTIVE_SIDECOMMAND_Y,      0, 0, 0xFF, SCREEN_WRITE_TRANSLUCENT);
            rcp_screen_full_write(gdl, sActiveSidekickCommandIcon, ACTIVE_SIDECOMMAND_ICON_X, ACTIVE_SIDECOMMAND_ICON_Y, 0, 0, 0xFF, SCREEN_WRITE_TRANSLUCENT);
        }
    }

    // @recomp: Reset alignment for energy bar
    gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXSetViewportAlign((*gdl)++, G_EX_ORIGIN_NONE, 0, 0);

    //Call the energy bar's draw
    cmdmenu_draw_energy_bar(gdl);

    // @recomp: Align command menu to right
    gEXSetViewportAlign((*gdl)++, G_EX_ORIGIN_RIGHT, -SCREEN_WIDTH * 4, 0);
    gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -SCREEN_WIDTH * 4, 0, -SCREEN_WIDTH * 4, 0);

    //Draw the inventory's vertical icon strip
    //(i.e. every part of the scroll except its top/bottom rolls)
    if (sDisplayedItemCount != 0) {
        if (dInventoryOpacity != 0) {
            /* 
                Figure out the number of tiles to draw (minimum of 3).

                Empty tiles are inserted to pad out the strip when 
                there're only 1/2/4 items shown on the page.
            */
            tileCount = 3;
            if (sDisplayedItemCount <= 3) {
                numSlotsAboveSelected = 1;
            } else {
                tileCount = 5;
                numSlotsAboveSelected = 2;
            }

            //Set a scissor mask for the inner strip of the inventory scroll
            cmdmenu_gfx_set_scroll_scissor(gdl);

            sTempIcon->y = 0;

            //Calculate the screen Y-coord of the top of the tile strip
            stripY = MENU_SCROLL_CENTRE_Y - (tileCount * (MENU_ITEM_HEIGHT/2));

            sTempIcon[1].tex = NULL;

            //Set the slots that show icons
            for (i = 0; i < sDisplayedItemCount; i++) {
                slot[i] = SLOT_OCCUPIED;
            }
            //Set the padded slots (empty BG tiles)
            for (i = sDisplayedItemCount; i < tileCount; i++) {
                slot[i] = SLOT_PADDED;
            }

            //Change sDisplayedItemCount so it's at least the size of the tile strip (including empty slots)
            if (sDisplayedItemCount < tileCount) {
                sDisplayedItemCount = tileCount;
            }

            //While animating moving between items
            {
                //Shift the top of the strip up by 1 item
                if (sInventoryScrollOffset > 0) {
                    numSlotsAboveSelected++;
                    stripY -= MENU_ITEM_HEIGHT;
                    tileCount++;
                }
                //Or shift it up by 2 during large offsets (when wrapping from bottom-to-top)
                if (sInventoryScrollOffset > MENU_ITEM_HEIGHT) {
                    numSlotsAboveSelected++;
                    stripY -= MENU_ITEM_HEIGHT;
                    tileCount++;
                }
            }

            //Have the strip move with the bottom of the scroll (during its expanding/collapsing animation)
            stripY += sInventoryUnrollY - dInventoryUnrollMax;

            //Calculate the item index of the uppermost slot in the strip
            itemIdx = sMenuSelectedItemIdx - numSlotsAboveSelected;
            if (itemIdx < 0) {
                itemIdx += sDisplayedItemCount;
            }

            //Iterate down the strip, drawing the slots' tiles
            for (i = 0; i < tileCount; i++) {
                if (slot[itemIdx] == SLOT_OCCUPIED) {
                    sTempIcon->animProgress = 0;
                    iconOpacity = dInventoryOpacity;

                    //Shift the selected item's tile when scrolling's finished (does nothing: cases identical)
                    //May suggest Rare considered lifting/popping out the selected item slightly
                    if ((itemIdx == sMenuSelectedItemIdx) && (sInventoryScrollOffset == 0)) {
                        sTempIcon->x = 0;
                        sTempIcon->y = 0;
                    } else {
                        sTempIcon->x = 0;
                        sTempIcon->y = 0;
                    }

                    if (sMenuItemVisibilities[itemIdx] != FALSE) {
                        sTempIcon->tex = sMenuItemTextures[itemIdx];

                        //Draw icon
                        if (track_func_80041E08()) {
                            //Widescreen aspect
                            rcp_tile_write(
                                gdl, 
                                sTempIcon, 
                                MENU_ITEM_X - 1, 
                                stripY + (i * MENU_ITEM_HEIGHT) + sInventoryScrollOffset, 
                                0xFF, 0xFF, 0xFF, iconOpacity
                            );
                        } else {
                            //Standard aspect
                            rcp_tile_write(
                                gdl, 
                                sTempIcon, 
                                MENU_ITEM_X, 
                                stripY + (i * MENU_ITEM_HEIGHT) + sInventoryScrollOffset, 
                                0xFF, 0xFF, 0xFF, iconOpacity
                            );
                        }

                        //Draw quantity text (for stackable items)
                        if (sMenuItemQuantities[itemIdx] > 1) {
                            sTempIcon->tex = sInventoryStackNumbersTex;
                            sTempIcon->animProgress = (sMenuItemQuantities[itemIdx] - 2) << 8; //Numbers only shown from 2 onwards (up to 10)
                            rcp_tile_write(
                                gdl, 
                                sTempIcon, 
                                MENU_ITEM_QUANTITY_X, 
                                stripY + (i * MENU_ITEM_HEIGHT) + sInventoryScrollOffset + MENU_ITEM_QUANTITY_OFFSET_Y, 
                                0xFF, 0xFF, 0xFF, 0xFF
                            );
                        }
                    }
                } else {
                    //Draw empty tile
                    if (track_func_80041E08()) {
                        //Widescreen aspect
                        rcp_tile_write(
                            gdl, 
                            sTextureTiles[CMDMENU_TEX_00_Scroll_BG], 
                            MENU_ITEM_X - 1, 
                            stripY + (i * MENU_ITEM_HEIGHT) + sInventoryScrollOffset, 
                            0xFF, 0xFF, 0xFF, 0xFF
                        );
                    } else {
                        //Standard aspect
                        rcp_tile_write(
                            gdl, 
                            sTextureTiles[CMDMENU_TEX_00_Scroll_BG], 
                            MENU_ITEM_X, 
                            stripY + (i * MENU_ITEM_HEIGHT) + sInventoryScrollOffset, 
                            0xFF, 0xFF, 0xFF, 0xFF
                        );
                    }
                }

                //Increment/wrap the item index
                itemIdx++;
                if (itemIdx >= sDisplayedItemCount) {
                    itemIdx -= sDisplayedItemCount;
                }
            }

            //Draw a selection square around the currently highlighted item
            rcp_tile_write(gdl, sTextureTiles[CMDMENU_TEX_31_Highlight_Corner_Top_Left],     ITEM_HL_X1, (sInventoryUnrollY - dInventoryUnrollMax) + ITEM_HL_Y1, 255, 255, 255, dInventoryOpacity);
            rcp_tile_write(gdl, sTextureTiles[CMDMENU_TEX_32_Highlight_Corner_Top_Right],    ITEM_HL_X2, (sInventoryUnrollY - dInventoryUnrollMax) + ITEM_HL_Y1, 255, 255, 255, dInventoryOpacity);
            rcp_tile_write(gdl, sTextureTiles[CMDMENU_TEX_33_Highlight_Corner_Bottom_Left],  ITEM_HL_X1, (sInventoryUnrollY - dInventoryUnrollMax) + ITEM_HL_Y2, 255, 255, 255, dInventoryOpacity);
            rcp_tile_write(gdl, sTextureTiles[CMDMENU_TEX_34_Highlight_Corner_Bottom_Right], ITEM_HL_X2, (sInventoryUnrollY - dInventoryUnrollMax) + ITEM_HL_Y2, 255, 255, 255, dInventoryOpacity);
            
            //Restore full-screen scissor
            cmdmenu_gfx_set_screen_scissor(gdl);
        }

        //Get page icon (Bag/SpellBook/Kyte/Tricky)
        if (dInventoryShow || 
            dInventoryOpacity == MAX_OPACITY || 
            (dInventoryOpacity != 0 && dOpacitySidekickMeter == 0)
        ) {
            switch (sInventoryPageID) {
            case CMDMENU_PAGE_7_Sidekick_Tricky:
                pageIcon = CMDMENU_TEX_42_Tricky;
                offsetY = 3;
                break;
            case CMDMENU_PAGE_8_Sidekick_Kyte:
                pageIcon = CMDMENU_TEX_54_Kyte;
                break;
            case CMDMENU_PAGE_6_Spells:
                offsetX = -2;
                offsetY = 9;
                pageIcon = CMDMENU_TEX_49_MagicBook;
                break;
            default:
            case CMDMENU_PAGE_0_Items_Krystal:
            case CMDMENU_PAGE_1_Items_Sabre:
            case CMDMENU_PAGE_2_Food_Actions_Krystal:
            case CMDMENU_PAGE_3_Food_Actions_Sabre:
            case CMDMENU_PAGE_4_Food_Krystal:
            case CMDMENU_PAGE_5_Food_Sabre:
                offsetX = 1;
                offsetY = 9;
                pageIcon = CMDMENU_TEX_50_Bag;
                break;
            }

            if (dOpacitySidekickMeter < dInventoryOpacity) {
                iconOpacity = dInventoryOpacity;
            } else {
                iconOpacity = dOpacitySidekickMeter;
            }
        } else {
            //Show sidekick's icon when the sidekick meter should be visible
            if (dOpacitySidekickMeter != 0) {
                pageIcon = CMDMENU_TEX_42_Tricky;
                if (sidekick != NULL && sidekick->id == OBJ_Kyte) {
                    pageIcon = CMDMENU_TEX_54_Kyte;
                    iconOpacity = dOpacitySidekickMeter;
                } else {
                    offsetY = 3;
                    iconOpacity = dOpacitySidekickMeter;
                }
            } else {
                iconOpacity = 0;
            }
        }

        //Draw page icon
        if (iconOpacity) {
            dInventoryPageIcon = tex_load_deferred(dTextableIDs[pageIcon]);
            rcp_screen_full_write(
                gdl, 
                dInventoryPageIcon, 
                PAGE_ICON_X + offsetX,
                PAGE_ICON_Y + offsetY,
                0, 
                0, 
                iconOpacity, 
                SCREEN_WRITE_TRANSLUCENT
            );
            tex_free(dInventoryPageIcon);
        }
    }

    // @recomp: Reset alignment
    gEXSetRectAlign((*gdl)++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXSetViewportAlign((*gdl)++, G_EX_ORIGIN_NONE, 0, 0);
}
