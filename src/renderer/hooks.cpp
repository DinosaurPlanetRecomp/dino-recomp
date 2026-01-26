#include "hooks.hpp"

#include <vector>

#include "plume_render_interface.h"
#include "rt64_render_hooks.h"

namespace dino::renderer {

struct Hook {
    RT64::RenderHookInit *init;
    RT64::RenderHookDraw *draw;
    RT64::RenderHookDeinit *deinit;
};

static bool hook_setup = false;
static std::vector<Hook> hooks = {};

static void set_rt64_hook();

void add_hook(RT64::RenderHookInit *init, RT64::RenderHookDraw *draw, RT64::RenderHookDeinit *deinit) {
    if (!hook_setup) {
        set_rt64_hook();
    }

    hooks.push_back(Hook {
        .init = init,
        .draw = draw,
        .deinit = deinit
    });
}

static void init_hook(plume::RenderInterface* _interface, plume::RenderDevice* device) {
    for (const auto& hook : hooks) {
        hook.init(_interface, device);
    }
}

static void draw_hook(plume::RenderCommandList* command_list, plume::RenderFramebuffer* swap_chain_framebuffer) {
    for (const auto& hook : hooks) {
        hook.draw(command_list, swap_chain_framebuffer);
    }
}

static void deinit_hook() {
    for (const auto& hook : hooks) {
        hook.deinit();
    }
}

static void set_rt64_hook() {
    hook_setup = true;

    RT64::SetRenderHooks(init_hook, draw_hook, deinit_hook);
}

}
