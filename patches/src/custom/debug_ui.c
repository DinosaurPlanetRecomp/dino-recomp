#include "patches.h"
#include "dbgui.h"

RECOMP_EXPORT void dbgui_textf(const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    dbgui_text(recomp_vsprintf_helper(fmt, args));

    va_end(args);
}

RECOMP_EXPORT void dbgui_label_textf(const char *label, const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);

    dbgui_label_text(label, recomp_vsprintf_helper(fmt, args));

    va_end(args);
}
