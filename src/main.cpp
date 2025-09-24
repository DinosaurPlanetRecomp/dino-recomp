#include <cstdio>
#include <cassert>
#include <vector>

#include "nfd.h"
#include "ultramodern/ultramodern.hpp"
#include "librecomp/game.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>
#endif

#include "input/input.hpp"
#include "input/controls.hpp"
#include "renderer/renderer.hpp"
#include "config/config.hpp"
#include "recomp_api/debug_ui_api.hpp"
#include "recomp_api/general_api.hpp"
#include "recomp_api/recomp_data_api.hpp"
#include "common/sdl.hpp"
#include "ui/recomp_ui.h"

#include "runtime/audio.hpp"
#include "runtime/gfx.hpp"
#include "runtime/mods.hpp"
#include "runtime/overlays.hpp"
#include "runtime/support.hpp"
#include "runtime/patches.hpp"
#include "runtime/preload.hpp"
#include "runtime/rom_patcher.hpp"
#include "runtime/rsp.hpp"
#include "runtime/threads.hpp"

const std::string version_string = "0.1.2";

extern "C" void recomp_entrypoint(uint8_t *rdram, recomp_context *ctx);
gpr get_entrypoint_address();

void dino_thread_create_callback(uint8_t *rdram, recomp_context *ctx) {
    // Dinosaur Planet modifies osCreateThread to enable MIPS3 float mode
    const uint32_t SR_FR = 0x04000000;
    cop0_status_write(ctx, SR_FR);
}

// array of supported GameEntry objects
std::vector<recomp::GameEntry> supported_games = {
    {
        .rom_hash = 0xB231A00966BE1430,
        .internal_name = "DINO PLANET",
        .game_id = u8"dino",
        .mod_game_id = "dino-planet",
        .save_type = recomp::SaveType::Flashram,
        .is_enabled = true,
        // Note: This is *not decompression*! We need to patch DLL $gp prologues for the
        // live recompiler, just like the patched ROM needed to compile Dinosaur Planet recomp.
        .decompression_routine =  dino::runtime::patch_rom,
        .has_compressed_code = true,
        .entrypoint_address = get_entrypoint_address(),
        .entrypoint = recomp_entrypoint,
        .thread_create_callback = dino_thread_create_callback,
    },
};

struct CliArgs {
    bool skip_launcher = false;
    bool show_console = false;
};

int main(int argc, char** argv) {
    // Parse CLI args
    CliArgs cli_args;
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "--skip-launcher") == 0) {
            cli_args.skip_launcher = true;
        } else if (strcmp(argv[i], "--show-console") == 0) {
            cli_args.show_console = true;
        }
    }

    // Load project version
    recomp::Version project_version{};
    if (!recomp::Version::from_string(version_string, project_version)) {
        ultramodern::error_handling::message_box(("Invalid version string: " + version_string).c_str());
        return EXIT_FAILURE;
    }

    // Map this executable into memory and lock it, which should keep it in physical memory. This ensures
    // that there are no stutters from the OS having to load new pages of the executable whenever a new code page is run.
    dino::runtime::PreloadContext preload_context;
    bool preloaded = preload_executable(preload_context);

    if (!preloaded) {
        fprintf(stderr, "Failed to preload executable!\n");
    }

#ifdef _WIN32
    // Set up high resolution timing period.
    timeBeginPeriod(1);

    // Allocate console on Windows if requested
    if (cli_args.show_console) {
        if (GetConsoleWindow() == nullptr) {
            AllocConsole();
            freopen("CONIN$", "r", stdin);
            freopen("CONOUT$", "w", stderr);
            freopen("CONOUT$", "w", stdout);
        }
    }

    // Set up console output to accept UTF-8 on windows
    SetConsoleOutputCP(CP_UTF8);

    // Change to a font that supports Japanese characters
    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof cfi;
    cfi.nFont = 0;
    cfi.dwFontSize.X = 0;
    cfi.dwFontSize.Y = 16;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy_s(cfi.FaceName, L"NSimSun");
    SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);

    // Force wasapi on Windows, as there seems to be some issue with sample queueing with directsound currently.
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif

#if defined(__linux__) && defined(RECOMP_FLATPAK)
    // When using Flatpak, applications tend to launch from the home directory by default.
    // Mods might use the current working directory to store the data, so we switch it to a directory
    // with persistent data storage and write permissions under Flatpak to ensure it works.
    std::error_code ec;
    std::filesystem::current_path("/var/data", ec);
#endif

    // Initialize SDL audio and set the output frequency.
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    dino::runtime::reset_audio(48000);

    // Source controller mappings file
    std::string controller_mappings_path = (dino::runtime::get_program_path() / "gamecontrollerdb.txt").string();
    if (SDL_GameControllerAddMappingsFromFile(controller_mappings_path.c_str()) < 0) {
        fprintf(stderr, "Failed to load controller mappings: %s\n", SDL_GetError());
    }

    recomp::register_config_path(dino::config::get_app_folder_path());
    
    // Register supported games
    for (const auto& game : supported_games) {
        recomp::register_game(game);
    }

    recompui::register_ui_exports();
    dino::recomp_api::register_general_exports();
    dino::recomp_api::register_data_api_exports();
    dino::recomp_api::register_debug_ui_exports();

    dino::runtime::register_overlays();
    dino::runtime::register_patches();

    dino::config::load_config();

    recomp::rsp::callbacks_t rsp_callbacks{
        .get_rsp_microcode = dino::runtime::get_rsp_microcode,
    };

    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = dino::renderer::create_render_context,
    };

    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx = dino::runtime::create_gfx,
        .create_window = dino::runtime::create_window,
        .update_gfx = dino::runtime::update_gfx,
    };

    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples = dino::runtime::queue_samples,
        .get_frames_remaining = dino::runtime::get_frames_remaining,
        .set_frequency = dino::runtime::set_frequency,
    };

    ultramodern::input::callbacks_t input_callbacks{
        .poll_input = dino::input::poll_inputs,
        .get_input = dino::input::get_n64_input,
        .set_rumble = dino::input::set_rumble,
        .get_connected_device_info = dino::input::get_connected_device_info,
    };

    ultramodern::events::callbacks_t thread_callbacks{
        .vi_callback = dino::input::update_rumble,
        .gfx_init_callback = recompui::update_supported_options,
    };

    ultramodern::error_handling::callbacks_t error_handling_callbacks{
        .message_box = recompui::message_box,
    };

    ultramodern::threads::callbacks_t threads_callbacks{
        .get_game_thread_name = dino::runtime::get_game_thread_name,
    };

    dino::runtime::register_mods();

    if (cli_args.skip_launcher) {
        // HACK: This relies on unsupported behavior in the runtime and requires using a fork
        //       of the runtime with a hack to avoid an uninitialized VI origin when doing this
        recomp::start_game(supported_games[0].game_id);
        recompui::hide_all_contexts();
    }

    recomp::start(
        project_version,
        {},
        rsp_callbacks,
        renderer_callbacks,
        audio_callbacks,
        input_callbacks,
        gfx_callbacks,
        thread_callbacks,
        error_handling_callbacks,
        threads_callbacks
    );

#ifdef _WIN32
    // End high resolution timing period.
    timeEndPeriod(1);
#endif

    // Note: NFD_Init is done in create_gfx, not main
    NFD_Quit();

    if (preloaded) {
        release_preload(preload_context);
    }

    return EXIT_SUCCESS;
}
