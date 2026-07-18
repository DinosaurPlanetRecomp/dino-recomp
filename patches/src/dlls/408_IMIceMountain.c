#include "patches.h"
#include "recomp_scheduler.h"

#include "dlls/objects/210_player.h"
#include "dlls/objects/408_IMIceMountain.h"
#include "dlls/objects/common/vehicle.h"
#include "game/objects/object.h"
#include "game/gamebits.h"
#include "sys/objects.h"
#include "sys/vi.h"
#include "dll.h"

#include "recomp/dlls/objects/408_IMIceMountain_recomp.h"

RECOMP_PATCH void IMIceMountain_do_race(Object *self, IMIceMountain_Data *objdata) {
    s32 racePosition;
    Object *player;
    Object *snowbike;

    gDLL_1_cmdmenu->vtbl->disable_buttons(L_CBUTTONS | R_CBUTTONS | D_CBUTTONS);
    // @recomp: Don't force 20 FPS if 60 hz gameplay is enabled
    if (!recomp_get_60fps_enabled()) {
        viSetUpdateRateTarget(3); // 20 FPS
    }
    if (mainGetBits(BIT_IM_Race_Ended)) {
        mainSetBits(BIT_IM_Race_Ended, 0);
        mainSetBits(BIT_IM_Race_Started, 0);
        player = objGetPlayer();
        snowbike = ((DLL_210_Player*)player->dll)->vtbl->get_vehicle(player);
        if (snowbike) {
            racePosition = dll_vehicle(snowbike)->GetRacePosition(snowbike);
        } else {
            racePosition = 0;
        }

        gDLL_29_Gplay->vtbl->set_obj_group_status(self->mapID, IM_ObjGroup1, 1);
        
        if (racePosition == 1) {
            gDLL_1_cmdmenu->vtbl->disable_buttons(R_CBUTTONS);
            objdata->state = STATE_Race_Won;
            mainSetBits(BIT_Play_Seq_00EA_IM_Sabre_Falls_Into_Hot_Spring, 1);
        } else {
            objdata->state = STATE_Race_Lost;
            mainSetBits(BIT_Play_Seq_00EB_IM_Lose_Race, 1);
        }
    }
}
