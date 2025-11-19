#include "dbgui.h"

// TODO: use decomp def
extern void warpPlayer(s32 warpIndex, s32 param2);

struct WarpEntry {
    s32 idx;
    char *name;
};

static struct WarpEntry warpEntries[] = {
    //{ 0, "0" }, // Exists but drops into void
    { 2, "2: Ice Mountain | Sabre's campsite cave" },
    { 5, "5: Warlock Mountain | Sabre-side transporter" },
    { 6, "6: Warlock Mountain | Krystal-side transporter" },
    { 12, "12: CloudRunner Fortress | Prison cell" },
    { 14, "14: SwapStone Circle | Rubble podium" },
    { 15, "15: SwapStone Hollow | Rocky podium" },
    { 25, "25: Warlock Mountain | Skydock" },
    { 26, "26: Ice Mountain | Summit" },
    { 29, "29: DarkIce Mines | Galadon boss room" },
    { 30, "30: DarkIce Mines | Galadon's stomach" },
    { 31, "31: Golden Plains | Transporter to Test of Knowledge" },
    { 34, "34: Warlock Mountain | Krystal-side | Spirit 1 deposit point" },
    { 35, "35: Warlock Mountain | Sabre-side | Near Spirit 2 deposit point" },
    { 36, "36: Test Of Combat" },
    { 37, "37: Test of Sacrifice" },
    { 38, "38: Test of Strength" },
    { 39, "39: Test of Magic" },
    { 40, "40: Test of Skill" },
    { 41, "41: Test of Character" },
    { 42, "42: Test of Knowledge" },
    { 43, "43: Test of Fear" },
    { 48, "48: Warlock Mountain | Krystal-side | Main Chamber" },
    { 49, "49: Warlock Mountain | Sabre-side | Spirit 2 deposit point" },
    { 50, "50: Warlock Mountain | Sabre-side | Outside orrery door" },
    { 51, "51: Warlock Mountain | Sabre-side | Main Chamber" },
    { 52, "52: Warlock Mountain | Krystal-side | Main Chamber | Spirit 3 deposit point" },
    { 53, "53: Warlock Mountain | Sabre-side | Main Chamber (2)" },
    { 54, "54: DarkIce Mines | Galadon boss room (2)" },
    { 55, "55: DarkIce Mines | Outside entrance" },
    { 56, "56: Warlock Mountain | Sabre-side | Port" },
    { 57, "57: Warlock Mountain | Sabre-side | Krazoa statue corridor" },
    { 58, "58: Warlock Mountain | Sabre-side | Main Chamber | Spirit 4 deposit point" },
    { 59, "59: Warlock Mountain | Sabre-side | Main Chamber" },
    { 60, "60: CloudRunner Fortress | Galleon deck" },
    { 61, "61: CloudRunner Fortress | Inside Galleon" },
    { 63, "63: Warlock Mountain | Krystal-side | Main Chamber (Upper) | Spirit 5 deposit point" },
    { 64, "64: Discovery Falls | Transporter to Test of Combat" },
    { 66, "66: Diamond Bay | Transporter to Test of Strength" },
    //{ 68, "68" }, // Exists but drops into void
    { 73, "73: Moon Mountain Pass | Transporter to Test of Magic" },
    { 76, "76: Warlock Mountain | Main Chamber (Upper)" },
    { 78, "78: Warlock Mountain | Sabre-side | Main Chamber (Upper) | Spirit 6 deposit point" },
    { 80, "80: SwapStone Circle | LightFoot Village" },
    { 81, "81: Volcano Force Point Temple | Caldera (Lower Floor)" },
    { 82, "82: SwapStone Hollow | Path to Walled City" },
    { 85, "85: SwapStone Circle | Pond outside Discovery Falls" },
    { 86, "86: SwapStone Shop entrance" },
    { 87, "87: SnowHorn Wastes | Inside the Magic Cave" },
    { 88, "88: Dragon Rock (Lower)" },
    { 90, "90: Walled City | Inside Klanadack boss room" },
    { 91, "91: Walled City | Outside Klanadack boss room" },
    { 92, "92: DarkIce Mines | Galadon boss room" },
    { 95, "95: SnowHorn Wastes | Outside the Magic Cave" }
};
const u32 WARP_ENTRIES_COUNT = sizeof(warpEntries) / sizeof(struct WarpEntry);
struct WarpEntry *selectedWarpEntry = &warpEntries[0];

void dbgui_warp_cheat_window(s32 *open) {
    if (dbgui_begin("Warp Cheat", open)) {
        if (dbgui_begin_combo("Warp Location", selectedWarpEntry->name)) {
            for (u32 i = 0; i < WARP_ENTRIES_COUNT; i++) {
                struct WarpEntry *entry = &warpEntries[i];
                if (dbgui_selectable(entry->name, entry == selectedWarpEntry)) {
                    selectedWarpEntry = entry;
                }
            }
            dbgui_end_combo();
        }

        if (dbgui_button("Warp")) {
            warpPlayer(selectedWarpEntry->idx, /*fade*/FALSE);
        }
    }
    dbgui_end();
}
