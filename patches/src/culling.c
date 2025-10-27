#include "recomp_funcs.h"
#include "patches.h"

#include "PR/ultratypes.h"
#include "sys/gfx/gx.h"

f32 recomp_get_frustum_aspect_hack() {
    f32 recompAspectRatio = recomp_get_aspect_ratio();

    // The game usually takes the FOV and divides it by two when creating the culling frustum for 4:3,
    // so we can use some ratios to plug in any aspect ratio.
    return 2.0f * ((4.0f / 3.0f) / recompAspectRatio);
}

RECOMP_PATCH int vi_contains_point(s32 x, s32 y) {
    // @recomp: Adjust for recomp aspect ratio. The game thinks we're running at 4:3 so we need this
    // to return true for negative x values and x values greater than 320, depending on the recomp screen size.
    // TODO: doesnt seem to work for everything...
    u32 gameResWidth = gCurrentResolutionH[gFramebufferChoice];
    u32 gameResHeight = gCurrentResolutionV[gFramebufferChoice];

    s32 ulx = 0;
    s32 uly = 0;
    s32 lrx = (s32)gameResWidth;
    s32 lry = (s32)gameResHeight;

    f32 widthScale = (recomp_get_aspect_ratio() / (4.0f / 3.0f)) - 1.0f;
    s32 adjust = (s32)((gameResWidth * widthScale) / 2.0f);
    ulx -= adjust;
    lrx += adjust;

    return x >= ulx && x < lrx
        && y >= uly && y < lry;
}
