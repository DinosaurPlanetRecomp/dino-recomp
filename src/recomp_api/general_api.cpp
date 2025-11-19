#ifdef _WIN32
#include <intrin.h>
#endif

#include "ultramodern/config.hpp"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/error_handling.hpp"
#include "librecomp/helpers.hpp"
#include "recomp.h"

#include "config/config.hpp"
#include "ui/recomp_ui.h"
#include "common.hpp"

extern "C" void recomp_get_window_resolution(uint8_t* rdram, recomp_context* ctx) {
    int width, height;
    recompui::get_window_size(width, height);

    gpr width_out = _arg<0, PTR(u32)>(rdram, ctx);
    gpr height_out = _arg<1, PTR(u32)>(rdram, ctx);

    MEM_W(0, width_out) = (u32)width;
    MEM_W(0, height_out) = (u32)height;
}

extern "C" void recomp_get_aspect_ratio_mode(uint8_t* rdram, recomp_context* ctx) {
    int ar = static_cast<int>(ultramodern::renderer::get_graphics_config().ar_option);

    _return(ctx, ar);
}

extern "C" void recomp_get_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::renderer::AspectRatio ar = ultramodern::renderer::get_graphics_config().ar_option;

    switch (ar) {
        case ultramodern::renderer::AspectRatio::Original:
        default:
            _return<float>(ctx, 4.0f / 3.0f);
            break;
        case ultramodern::renderer::AspectRatio::Expand:
            int width, height;
            recompui::get_window_size(width, height);

            _return<float>(ctx, std::max((float)width / (float)height, (4.0f / 3.0f)));
            break;
    }
}

extern "C" void recomp_get_hud_ratio_mode(uint8_t* rdram, recomp_context* ctx) {
    int hr = static_cast<int>(ultramodern::renderer::get_graphics_config().hr_option);

    _return(ctx, hr);
}

extern "C" void recomp_get_refresh_rate(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::renderer::RefreshRate rr = ultramodern::renderer::get_graphics_config().rr_option;

    switch (rr) {
        case ultramodern::renderer::RefreshRate::Display:
            _return<int>(ctx, ultramodern::get_display_refresh_rate());
            break;
        case ultramodern::renderer::RefreshRate::Manual:
            _return<int>(ctx, ultramodern::renderer::get_graphics_config().rr_manual_value);
            break;
        case ultramodern::renderer::RefreshRate::Original:
        default:
            _return<int>(ctx, dino::config::get_sixty_fps_enabled() ? 60 : 30);
            break;
    }
}

extern "C" void recomp_error_message_box(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) message_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *message = dino::recomp_api::copy_rdram_str(message_ptr, rdram, ctx);

    ultramodern::error_handling::message_box(message);

    free(message);
}

extern "C" void recomp_exit_with_error(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) message_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *message = dino::recomp_api::copy_rdram_str(message_ptr, rdram, ctx);

    ultramodern::error_handling::message_box(message);

    free(message);

    // Breakpoint in debug builds so we can get a full stack trace
#ifndef NDEBUG
#if _WIN32
    __debugbreak();
#elif defined(__linux__)
    __builtin_trap();
#endif
#endif

    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, __FUNCTION__);
}

extern "C" void recomp_exit(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::quit();
}

extern "C" void recomp_powf(uint8_t* rdram, recomp_context* ctx) {
    float a = _arg<0, float>(rdram, ctx);
    float b = ctx->f14.fl; //_arg<1, float>(rdram, ctx);

    _return(ctx, std::pow(a, b));
}

extern "C" void recomp_time_us(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<u32>(std::chrono::duration_cast<std::chrono::microseconds>(ultramodern::time_since_start()).count()));
}

extern "C" void recomp_get_60fps_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, dino::config::get_sixty_fps_enabled() ? 1 : 0);
}

extern "C" void recomp_get_bgm_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, dino::config::get_bgm_volume());
}

extern "C" void recomp_get_sfx_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, dino::config::get_sfx_volume());
}

extern "C" void recomp_get_dialog_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, dino::config::get_dialog_volume());
}

extern "C" void recomp_get_subtitles_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, dino::config::get_subtitles_enabled() ? 1 : 0);
}

namespace dino::recomp_api {
    void register_general_exports() {
        REGISTER_EXPORT(recomp_get_window_resolution);
        REGISTER_EXPORT(recomp_get_aspect_ratio_mode);
        REGISTER_EXPORT(recomp_get_aspect_ratio);
        REGISTER_EXPORT(recomp_get_hud_ratio_mode);
        REGISTER_EXPORT(recomp_get_refresh_rate);
        REGISTER_EXPORT(recomp_error_message_box);
        REGISTER_EXPORT(recomp_exit_with_error);
        REGISTER_EXPORT(recomp_exit);
        REGISTER_EXPORT(recomp_powf);
        REGISTER_EXPORT(recomp_time_us);
        REGISTER_EXPORT(recomp_get_60fps_enabled);
        REGISTER_EXPORT(recomp_get_bgm_volume);
        REGISTER_EXPORT(recomp_get_sfx_volume);
        REGISTER_EXPORT(recomp_get_dialog_volume);
        REGISTER_EXPORT(recomp_get_subtitles_enabled);
    }
}
