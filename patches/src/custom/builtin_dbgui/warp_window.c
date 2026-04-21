#include "patches.h"
#include "dbgui.h"

#include "sys/fs.h"
#include "sys/main.h"
#include "sys/map.h"
#include "sys/memory.h"

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

static const char* playerNames[] = {"Sabre", "Krystal"};

static const char **mapNames;
static s32 *mapNumSetups;
static s32 numMaps;
static s32 loadedMaps = FALSE;

static void load_maps(void) {
    if (!loadedMaps) {
        numMaps = get_file_size(MAPINFO_BIN) / 0x20;
        void *mapInfo = read_alloc_file(MAPINFO_BIN, 0);
        mapNames = recomp_alloc(numMaps * sizeof(const char*));
        for (s32 i = 0; i < numMaps; i++) {
            char *name = recomp_alloc(29 * sizeof(char));
            bcopy((void*)((u32)mapInfo + (0x20 * i)), name, 28);
            name[28] = '\0';

            mapNames[i] = name;
        }
        mmFree(mapInfo);

        s16 *mapSetups = read_alloc_file(MAPSETUP_IND, 0);
        mapNumSetups = recomp_alloc(numMaps * sizeof(s32));
        for (s32 i = 0; i < numMaps; i++) {
            mapNumSetups[i] = mapSetups[i + 1] - mapSetups[i];
        }
        mmFree(mapSetups);

        loadedMaps = TRUE;
    }
}

static const char* get_map_name(s32 mapID) {
    return mapID >= 0 && mapID < numMaps ? mapNames[mapID] : "<null>";
}

void dbgui_warp_window(s32 *open) {
    if (dbgui_begin("Warp", open)) {
        load_maps();
        
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

        dbgui_separator();

        static s32 mapID = 0;
        static s32 setupID = 0;
        static s32 playerNo = 0;
        if (dbgui_begin_combo("Map", get_map_name(mapID))) {
            for (s32 i = 0; i < numMaps; i++) {
                if (dbgui_selectable(recomp_sprintf_helper("%d: %s", i, get_map_name(i)), i == mapID)) {
                    mapID = i;
                    setupID = 0;
                }
            }
            dbgui_end_combo();
        }
        if (dbgui_input_int("Setup ID", &setupID)) {
            if (setupID < 0) setupID = 0;
            if (setupID >= mapNumSetups[mapID]) {
                if (mapNumSetups[mapID] == 0) {
                    setupID = 0;
                } else {
                    setupID = mapNumSetups[mapID] - 1;
                }
            }
        }
        if (dbgui_begin_combo("Player", playerNames[playerNo])) {
            for (s32 i = 0; i < 2; i++) {
                if (dbgui_selectable(playerNames[i], i == playerNo)) {
                    playerNo = i;
                }
            }
            dbgui_end_combo();
        }

        if (dbgui_button("Change Map")) {
            func_800141A4(mapID, setupID, playerNo, 0);
        }
    }
    dbgui_end();
}
