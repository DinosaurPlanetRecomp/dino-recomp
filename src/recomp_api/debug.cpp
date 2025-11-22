#include "ultramodern/ultramodern.hpp"
#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "config/config.hpp"

extern "C" void recomp_puts(uint8_t* rdram, recomp_context* ctx) {
    if (!dino::config::get_debug_stdout_enabled()) {
        return;
    }

    PTR(char) cur_str = _arg<0, PTR(char)>(rdram, ctx);
    u32 length = _arg<1, u32>(rdram, ctx);

    for (u32 i = 0; i < length; i++) {
        fputc(MEM_B(i, (gpr)cur_str), stdout);
    }
}

extern "C" void recomp_eputs(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) cur_str = _arg<0, PTR(char)>(rdram, ctx);
    u32 length = _arg<1, u32>(rdram, ctx);

    for (u32 i = 0; i < length; i++) {
        fputc(MEM_B(i, (gpr)cur_str), stderr);
    }
}

extern "C" void recomp_get_diprintf_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, dino::config::get_debug_diprintf_enabled() ? 1 : 0);
}

extern "C" void recomp_get_debug_dll_logging_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, dino::config::get_debug_stdout_enabled() && dino::config::get_debug_dll_logging_enabled() ? 1 : 0);
}
