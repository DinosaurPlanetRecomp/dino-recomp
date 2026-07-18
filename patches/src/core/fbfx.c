#include "patches.h"

#include "PR/ultratypes.h"
#include "PR/gbi.h"
#include "sys/framebuffer_fx.h"

extern s32 gFbfxEffectID;
extern s32 gFbfxEffectDuration;
extern s32 gFbfxTimer;

RECOMP_PATCH void fbfxTick(Gfx **gdl, s32 updateRate) {
    if (gFbfxEffectID > FBFX_NONE) {
        if (gFbfxEffectID == FBFX_MOTION_BLUR && gFbfxTimer == 0) {
            gFbfxTimer = gFbfxEffectDuration;
        }
        // @recomp: Don't run the original framebuffer FX code. It doesn't work at higher resolutions.
        //          We'll run our own implementation elsewhere.
        // fbfxDoEffect(gdl, gFbfxEffectDuration, gFbfxEffectID, camGetLetterbox());
        if (gFbfxEffectID == FBFX_MOTION_BLUR) {
            gFbfxTimer -= updateRate;
            if (gFbfxTimer <= 0) {
                gFbfxEffectID = FBFX_NONE;
                gFbfxTimer = 0;
            }
        } else {
            gFbfxEffectID = FBFX_NONE;
        }
    } else {
        gFbfxTimer = 0;
    }
}
