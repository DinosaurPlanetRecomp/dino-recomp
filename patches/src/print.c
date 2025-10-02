#include "stdarg.h"

#include "patches.h"
#include "recomp_funcs.h"

#include "sys/print.h"
#include "PR/sched.h"
#include "sys/gfx/gx.h"
#include "sys/gfx/texture.h"

extern int _Printf(void*, void*, const char* fmt, va_list args);

// Hook up game printf to recomp logging
RECOMP_PATCH void* proutSyncPrintf(void* dst, const char* buf, s32 size) {
    recomp_puts(buf, size);
    return (void*)1;
}

void* proutSyncEPrintf(void* dst, const char* buf, s32 size) {
    recomp_eputs(buf, size);
    return (void*)1;
}

RECOMP_EXPORT int recomp_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int ret = _Printf(&proutSyncPrintf, NULL, fmt, args);

    va_end(args);

    return ret;
}

RECOMP_EXPORT int recomp_vprintf(const char* fmt, va_list args) {
    return _Printf(&proutSyncPrintf, NULL, fmt, args);
}

RECOMP_EXPORT int recomp_eprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int ret = _Printf(&proutSyncEPrintf, NULL, fmt, args);

    va_end(args);

    return ret;
}

RECOMP_EXPORT int recomp_veprintf(const char* fmt, va_list args) {
    return _Printf(&proutSyncEPrintf, NULL, fmt, args);
}

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

static void *proutSprintf(void *dst, const char *buf, s32 size) {
    bcopy(buf, dst, size);
	return (void*)((char*)dst + size);
}

// The game's code has an sprintf and vsprintf implementation, but it has some special
// logic in it that may or may not cause issues. We'll provide a "normal" implementation just in case...
RECOMP_EXPORT int recomp_sprintf(char *s, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	
    int ret = _Printf(&proutSprintf, (void*)s, fmt, args);
	if (0 <= ret) {
		s[ret] = '\0';
    }
	
    va_end(args);
	
    return (ret);
}

RECOMP_EXPORT int recomp_vsprintf(char *s, const char *fmt, va_list args) {
    int ret = _Printf(&proutSprintf, (void*)s, fmt, args);
	if (0 <= ret) {
		s[ret] = '\0';
    }
	
    return (ret);
}

RECOMP_EXPORT const char *recomp_vsprintf_helper(const char *fmt, va_list args) {
    // 8KiB buffer for string formatting... if that's not enough then
    // you have bigger problems...
    static char buffer[1024 * 8];
    
    int ret = recomp_vsprintf(buffer, fmt, args);
    
    if (ret >= (int)sizeof(buffer)) {
        buffer[sizeof(buffer) - 1] = '\0';
        recomp_eprintf("ERROR recomp_vsprintf_helper buffer overflow by string:\n%s\n", buffer);
        recomp_exit_with_error("ERROR recomp_vsprintf_helper buffer overflow!!! Please pass a smaller string! See stderr for the problematic string.\n");
    }

    return buffer;
}

RECOMP_EXPORT const char *recomp_sprintf_helper(const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    const char *str = recomp_vsprintf_helper(fmt, args);

    va_end(args);

    return str;
}
