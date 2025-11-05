#pragma once

#include "imgui.h"

namespace dino::debug_ui::backend {

extern ImGuiContext *dino_imgui_ctx;
bool is_open();
bool in_ui_frame();

void begin();
void end();
void set_render_hooks();

}
