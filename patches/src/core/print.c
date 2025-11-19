#include "patches.h"
#include "recomp_funcs.h"

#include "stdarg.h"
#include "sys/print.h"
#include "sys/gfx/texture.h"

extern char gDebugPrintBufferStart[0x900];
extern char *gDebugPrintBufferEnd;
extern s8 D_800931AC;
extern s8 D_800931B0;
extern s8 D_800931B4;
extern s8 D_800931B8;
extern Texture *gDiTextures[3];

RECOMP_PATCH void diPrintfInit() {
    // @recomp: Remove code that scales up the rendered diPrintf text when
    // the resolution is above 320x240. This results in the text being way
    // too big for some reason.
    /*
    u32 fbRes;

    fbRes = get_some_resolution_encoded();
    if (RESOLUTION_WIDTH(fbRes) > 320) {
        D_800931AC = 1;
    }
    if (RESOLUTION_HEIGHT(fbRes) > 240) {
        D_800931B0 = 1;
    }
    */

    D_800931B4 = 0;
    D_800931B8 = 0;

    gDiTextures[0] = queue_load_texture_proxy(0);
    gDiTextures[1] = queue_load_texture_proxy(1);
    gDiTextures[2] = queue_load_texture_proxy(2);

    gDebugPrintBufferEnd = &gDebugPrintBufferStart[0];
}

// Patch diPrintf impl back in
RECOMP_PATCH int diPrintf(const char* fmt, ...) {
    va_list args;
    int written;

    if (!recomp_get_diprintf_enabled()) {
        return 0;
    }

    va_start(args, fmt);

    if ((gDebugPrintBufferEnd - gDebugPrintBufferStart) > 0x800) {
        recomp_eprintf("*** diPrintf Error *** ---> Out of string space. (Print less text!)\n");
        return -1;
    }

    sprintfSetSpacingCodes(TRUE);
    written = vsprintf(gDebugPrintBufferEnd, fmt, args);
    sprintfSetSpacingCodes(FALSE);

    if (written > 0) {
        gDebugPrintBufferEnd = &gDebugPrintBufferEnd[written] + 1;
    }

    va_end(args);

    return 0;
}
