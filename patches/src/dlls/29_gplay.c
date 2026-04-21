#include "patches.h"
#include "recomp_savedata.h"

#include "PR/ultratypes.h"
#include "dlls/engine/29_gplay.h"
#include "dlls/engine/31_flash.h"
#include "sys/asset_thread.h"
#include "sys/dll.h"
#include "sys/fs.h"
#include "sys/memory.h"
#include "dll.h"

#include "recomp/dlls/engine/29_gplay_recomp.h"

typedef struct {
    s8 mapID;
    s8 setupID;
} MapSetup;

typedef struct {
    s32 mapID;
    // Bitfield of the status of each object group
    s32 groupBits;
} MapObjGroupBits;

extern GplaySaveFlash *sSavegame;
extern Savegame *sRestartSave;
extern GplayOptions *sGameOptions;
extern GameState sState;
extern s8 sSavegameIdx;
extern u8 bss_1840[40];
extern u32 sAllMapObjGroups[120];
extern MapSetup sMapSetup;
extern MapObjGroupBits sMapObjGroups;

extern void gplay_start_loaded_game(void);
extern void gplay_reset_state(void);

RECOMP_PATCH void gplay_ctor(DLLFile *self)  {
    // @recomp: Allocate room for recomp flash data
    sSavegame = (GplaySaveFlash*)mmAlloc(sizeof(RecompFlashData), COLOUR_TAG_YELLOW, NULL);
    gplay_reset_state();
    sRestartSave = NULL;
    sGameOptions = (GplayOptions*)mmAlloc(sizeof(GplayOptions), COLOUR_TAG_YELLOW, NULL);
    sMapSetup.mapID = -1;
    sMapObjGroups.mapID = -1;
}

RECOMP_PATCH s32 gplay_load_save(s8 idx, u8 startGame) {
    u8 loadStatus0;
    u8 loadStatus1;
    GplaySaveFlash *copy1Ptr;
    s32 ret;

    if (idx != sSavegameIdx) {
        if (idx < MAX_SAVEGAMES) {
            sSavegameIdx = idx;

            // Depending on whether this savegame was saved an even or odd number of times determines
            // which copy is the main savegame and which is the backup. Load both and compare.
            // @recomp: Allocate room for recomp flash data
            copy1Ptr = (GplaySaveFlash*)mmAlloc(sizeof(RecompFlashData), COLOUR_TAG_YELLOW, NULL);

            // @recomp: Load full size with recomp data
            loadStatus0 = gDLL_31_Flash->vtbl->load_game(&sSavegame->asFlash, idx,                 sizeof(RecompFlashData), TRUE); // copy 0
            loadStatus1 = gDLL_31_Flash->vtbl->load_game(&copy1Ptr->asFlash,  idx + MAX_SAVEGAMES, sizeof(RecompFlashData), TRUE); // copy 1

            if (!loadStatus0) {
                if (!loadStatus1) {
                    // both copies of the savegame failed to load
                    mmFree(copy1Ptr);
                    ret = 0;
                } else {
                    // copy 0 failed but copy 1 is good, use that one
                    mmFree(sSavegame);
                    ret = 2;
                    sSavegame = copy1Ptr;
                }
            } else if (!loadStatus1) {
                // copy 1 failed but copy 0 is good, use that one
                mmFree(copy1Ptr);
                ret = 2; // bug? shouldn't this be 1 since copy 0 is being returned?
            } else if (copy1Ptr->asSave.file.timePlayed < sSavegame->asSave.file.timePlayed) {
                // both copies are good but copy 0 is newer, use that one
                mmFree(copy1Ptr);
                ret = 1;
            } else {
                // both copies are good but copy 1 is newer, use that one
                mmFree(sSavegame);
                ret = 2;
                sSavegame = copy1Ptr;
            }

            // @recomp: Load recomp savedata
            if (ret != 0) {
                recomp_savedata_load((RecompFlashData*)sSavegame, sSavegameIdx);
            }
        } else {
            // @recomp: Zero out recomp data when loading from SAVEGAME.bin
            bzero(sSavegame, sizeof(RecompFlashData));
            queue_load_file_region_to_ptr(
                (void**)&sSavegame->asFlash,
                SAVEGAME_BIN,
                idx * sizeof(FlashStruct) - (sizeof(FlashStruct) * 4),
                sizeof(FlashStruct));
            ret = 1;
        }
    } else {
        ret = 1;
    }

    if (startGame) {
        gplay_start_loaded_game();
    } else {
        bcopy(&sSavegame->asSave, &sState.save, sizeof(Savegame));
    }

    return ret;
}

RECOMP_PATCH void gplay_save_game(void) {
    if (sSavegameIdx != -1) {
        sState.save.file.numTimesSaved += 1;

        // Always stage & save data in the Savefile struct
        bcopy(&sState.save.file, &sSavegame->asSave.file, sizeof(Savefile));

        // @recomp: Write recomp savedata
        recomp_savedata_save((RecompFlashData*)sSavegame, sSavegameIdx);

        // Alternate save location in flash every time the gamesave is saved
        //
        // In flash, there are 8 gamesave slots but only 4 can be distinct gamesaves
        // as each gamesave alternates between two slots (either idx + 0 or idx + 4).
        //
        // | 0 | savegame 0 alternate 0 |
        // | 1 | savegame 1 alternate 0 |
        // | 2 | savegame 2 alternate 0 |
        // | 3 | savegame 3 alternate 0 |
        // | 4 | savegame 0 alternate 1 |
        // | 5 | savegame 1 alternate 1 |
        // | 6 | savegame 2 alternate 1 |
        // | 7 | savegame 3 alternate 1 |
        // @recomp: Save full size with recomp data
        gDLL_31_Flash->vtbl->save_game(
            &sSavegame->asFlash, 
            sSavegameIdx + (sSavegame->asSave.file.numTimesSaved % 2) * MAX_SAVEGAMES, 
            sizeof(RecompFlashData), 
            TRUE);
        
        if (sRestartSave != NULL) {
            bcopy(&sState.save.chkpnt, &sRestartSave->chkpnt, sizeof(CheckpointSaveData));
        }
    }
}

RECOMP_PATCH void gplay_reset_state(void) {
    sSavegameIdx = -1;
    bzero(&sState, sizeof(GameState));
    // @recomp: Erase recomp flash data as well
    bzero(sSavegame, sizeof(RecompFlashData));
}
