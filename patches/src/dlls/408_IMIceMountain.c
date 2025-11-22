#include "patches.h"
#include "recomp_funcs.h"

#include "dlls/objects/210_player.h"
#include "dlls/objects/common/vehicle.h"
#include "game/objects/object.h"
#include "game/gamebits.h"
#include "sys/gfx/gx.h"
#include "sys/objects.h"
#include "dll.h"

#include "recomp/dlls/objects/408_IMIceMountain_recomp.h"

typedef struct {
    u8 state;
    u8 flags;
    s8 warpCounter;
} IMIceMountain_Data;

typedef enum {
    STATE_0,
    STATE_Rescue_Tricky,
    STATE_Race_Start_Sequence,
    STATE_Race,
    STATE_Race_Won,
    STATE_Race_Lost,
    STATE_Intro_Sequence
} IMIceMountain_State;

RECOMP_PATCH void IMIceMountain_do_race(Object *self, IMIceMountain_Data *objdata) {
    s32 racePosition;
    Object *player;
    Object *snowbike;

    gDLL_1_UI->vtbl->func_2B8(7);
    // @recomp: Don't force 20 FPS if 60 hz gameplay is enabled
    if (!recomp_get_60fps_enabled()) {
        vi_set_update_rate_target(3); // 20 FPS
    }
    if (main_get_bits(BIT_IM_Race_Ended)) {
        main_set_bits(BIT_IM_Race_Ended, 0);
        main_set_bits(BIT_IM_Race_Started, 0);
        player = get_player();
        snowbike = ((DLL_210_Player*)player->dll)->vtbl->get_vehicle(player);
        if (snowbike) {
            racePosition = ((DLL_IVehicle*)snowbike->dll)->vtbl->func17(snowbike);
        } else {
            racePosition = 0;
        }
        gDLL_29_Gplay->vtbl->set_obj_group_status(self->mapID, 1, 1);
        if (racePosition == 1) {
            gDLL_1_UI->vtbl->func_2B8(1);
            objdata->state = STATE_Race_Won;
            main_set_bits(BIT_Play_Seq_00EA_IM_Sabre_Falls_Into_Hot_Spring, 1);
        } else {
            objdata->state = STATE_Race_Lost;
            main_set_bits(BIT_Play_Seq_00EB_IM_Lose_Race, 1);
        }
    }
}
