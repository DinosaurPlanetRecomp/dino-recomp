#include "gfx.hpp"

#include "nfd.h"
#include "ultramodern/ultramodern.hpp"

#include "input/input.hpp"
#include "common/error.hpp"
#include "common/sdl.hpp"

#include "../../lib/rt64/src/contrib/stb/stb_image.h"

namespace dino::runtime {

static SDL_Window* window;

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) > 0) {
        exit_error("Failed to initialize SDL2: %s\n", SDL_GetError());
    }

    fprintf(stdout, "SDL Video Driver: %s\n", SDL_GetCurrentVideoDriver());

    // Initialize native file dialogs.
    // Note: NFD suggests that it should only be initialized *after* SDL.
    NFD_Init();

    return {};
}

#if defined(__gnu_linux__)
#include "icon_bytes.h"

bool SetImageAsIcon(const char* filename, SDL_Window* window)
{
    // Read data
    int width, height, bytesPerPixel;
    void* data = stbi_load_from_memory(reinterpret_cast<const uint8_t*>(icon_bytes), sizeof(icon_bytes), &width, &height, &bytesPerPixel, 4);

    // Calculate pitch
    int pitch;
    pitch = width * 4;
    pitch = (pitch + 3) & ~3;

    // Setup relevance bitmask
    int Rmask, Gmask, Bmask, Amask;

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    Rmask = 0x000000FF;
    Gmask = 0x0000FF00;
    Bmask = 0x00FF0000;
    Amask = 0xFF000000;
#else
    Rmask = 0xFF000000;
    Gmask = 0x00FF0000;
    Bmask = 0x0000FF00;
    Amask = 0x000000FF;
#endif

    SDL_Surface* surface = nullptr;
    if (data != nullptr) {
        surface = SDL_CreateRGBSurfaceFrom(data, width, height, 32, pitch, Rmask, Gmask,
                            Bmask, Amask);
    }

    if (surface == nullptr) {   
        if (data != nullptr) {
            stbi_image_free(data);
        }
        return false;
	} else {
        SDL_SetWindowIcon(window,surface);
        SDL_FreeSurface(surface);
        stbi_image_free(data);
        return true;
    }
}
#endif

ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    uint32_t flags = SDL_WINDOW_RESIZABLE;

#if defined(RT64_SDL_WINDOW_VULKAN)
    flags |= SDL_WINDOW_VULKAN;
#endif

    window = SDL_CreateWindow("Dinosaur Planet: Recompiled", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320 * 4, 240 * 4, flags );
#if defined(__linux__)
    SetImageAsIcon("icons/512.png",window);
    if (ultramodern::renderer::get_graphics_config().wm_option == ultramodern::renderer::WindowMode::Fullscreen) { // TODO: Remove once RT64 gets native fullscreen support on Linux
        SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(window,0);
    }
#endif

    if (window == nullptr) {
        exit_error("Failed to create window: %s\n", SDL_GetError());
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

#if defined(_WIN32)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.win.window, GetCurrentThreadId() };
#elif defined(__linux__) || defined(__ANDROID__)
    return ultramodern::renderer::WindowHandle{ window };
#elif defined(__APPLE__)
    return ultramodern::renderer::WindowHandle{ wmInfo.info.cocoa.window, wmInfo.info.cocoa.view };
#else
    static_assert(false && "Unimplemented");
#endif
}

SDL_Window *get_window() {
    return window;
}

void update_gfx(void*) {
    dino::input::handle_events();
}

}
