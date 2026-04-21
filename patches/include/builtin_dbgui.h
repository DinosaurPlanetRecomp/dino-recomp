#pragma once

#include "PR/ultratypes.h"

void builtin_dbgui();
void builtin_dbgui_game_tick();

void dbgui_dlls_window(s32 *open);
void dbgui_warp_window(s32 *open);
void dbgui_player_cheat_window(s32 *open);
void dbgui_audio_window(s32 *open);
void dbgui_graphics_window(s32 *open);
void dbgui_memory_window(s32 *open);

void dbgui_player_cheat_game_tick();