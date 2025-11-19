#include "patches.h"
#include "recomp_funcs.h"

extern int _Printf(void*, void*, const char* fmt, va_list args);
extern void* proutSyncPrintf(void* str, const char* buf, size_t n);

static void* recomp_proutSyncEPrintf(void* dst, const char* buf, s32 size) {
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

    int ret = _Printf(&recomp_proutSyncEPrintf, NULL, fmt, args);

    va_end(args);

    return ret;
}

RECOMP_EXPORT int recomp_veprintf(const char* fmt, va_list args) {
    return _Printf(&recomp_proutSyncEPrintf, NULL, fmt, args);
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
