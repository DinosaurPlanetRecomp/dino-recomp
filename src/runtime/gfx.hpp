#pragma once

#include "ultramodern/ultramodern.hpp"

namespace dino::runtime {

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx();
ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t, int width, int height);
SDL_Window *get_window();
void update_gfx(void*);

}