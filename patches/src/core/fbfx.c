#include "patches.h"

#include "PR/ultratypes.h"
#include "PR/gbi.h"

extern s32 D_80092A50;
extern s32 D_80092A54;
extern s32 D_800B49E0;

RECOMP_PATCH void func_8003E9F0(Gfx** gdl, s32 updateRate) {
    s32 temp_v0;
    s32 *v0;

    if (D_80092A50 > 0) {
        if ((D_80092A50 == 10) && ((temp_v0 = D_800B49E0) == 0)) {
            D_800B49E0 = temp_v0 = D_80092A54;
        }
        // @recomp: Don't run the original framebuffer FX code. It doesn't work at higher resolutions.
        //          We'll run our own implementation elsewhere.
        // func_8003FE70(gdl, *D_80092A54, D_80092A50, func_80004A4C());
        v0 = &D_800B49E0;
        if (D_80092A50 == 10) {
            temp_v0 = D_800B49E0;
            temp_v0 -= updateRate;
            *v0 = temp_v0;
            if (*v0 <= 0) {
                D_80092A50 = 0;
                *v0 = 0;
            }
        } else {
            D_80092A50 = 0;
        }
        return;
    } else {
        D_800B49E0 = 0;
    }
}
