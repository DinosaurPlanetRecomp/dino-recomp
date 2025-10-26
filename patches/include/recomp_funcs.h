#pragma once

#include "patch_helpers.h"

DECLARE_FUNC(void, recomp_puts, const char* data, u32 size);
DECLARE_FUNC(void, recomp_eputs, const char* data, u32 size);
DECLARE_FUNC(s32, recomp_get_diprintf_enabled, void);

typedef enum {
    RECOMP_ASPECT_ORIGINAL,
    RECOMP_ASPECT_EXPAND,
    RECOMP_ASPECT_MANUAL
} RecompAspectRatio;

typedef enum {
    RECOMP_HUD_ORIGINAL,
    RECOMP_HUD_CLAMP16X9,
    RECOMP_HUD_FULL
} RecompHUDRatio;

DECLARE_FUNC(void, recomp_get_window_resolution, u32 *width, u32 *height);
DECLARE_FUNC(RecompAspectRatio, recomp_get_aspect_ratio_mode, void);
DECLARE_FUNC(f32, recomp_get_aspect_ratio, void);
DECLARE_FUNC(RecompHUDRatio, recomp_get_hud_ratio_mode, void);
DECLARE_FUNC(int, recomp_get_refresh_rate, void);
DECLARE_FUNC(s32, recomp_get_60fps_enabled, void);

DECLARE_FUNC(void, recomp_error_message_box, const char *message);
DECLARE_FUNC(void, recomp_exit_with_error, const char *message);
