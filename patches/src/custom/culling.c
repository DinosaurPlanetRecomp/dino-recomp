#include "recomp_funcs.h"

#include "PR/ultratypes.h"

f32 recomp_get_frustum_aspect_hack() {
    f32 recompAspectRatio = recomp_get_aspect_ratio();

    // The game usually takes the FOV and divides it by two when creating the culling frustum for 4:3,
    // so we can use some ratios to plug in any aspect ratio.
    return 2.0f * ((4.0f / 3.0f) / recompAspectRatio);
}
