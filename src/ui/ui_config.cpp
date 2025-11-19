#include "input/input.hpp"
#include "input/controls.hpp"
#include "config/config.hpp"
#include "runtime/support.hpp"
#include "runtime/gfx.hpp"
#include "renderer/renderer.hpp"

#include "ui/recomp_ui.h"
#include "promptfont.h"
#include "ultramodern/config.hpp"
#include "ultramodern/ultramodern.hpp"
#include "RmlUi/Core.h"

#include "core/ui_context.h"

ultramodern::renderer::GraphicsConfig new_options;
Rml::DataModelHandle nav_help_model_handle;
Rml::DataModelHandle general_model_handle;
Rml::DataModelHandle controls_model_handle;
Rml::DataModelHandle graphics_model_handle;
Rml::DataModelHandle sound_options_model_handle;

// True if controller config menu is open, false if keyboard config menu is open, undefined otherwise
bool configuring_controller = false;

int recompui::config_tab_to_index(recompui::ConfigTab tab) {
    switch (tab) {
    case recompui::ConfigTab::General:
        return 0;
    case recompui::ConfigTab::Controls:
        return 1;
    case recompui::ConfigTab::Graphics:
        return 2;
    case recompui::ConfigTab::Sound:
        return 3;
    case recompui::ConfigTab::Mods:
        return 4;
    case recompui::ConfigTab::Debug:
        return 5;
    default:
        assert(false && "Unknown config tab.");
        return 0;
    }
}

template <typename T>
void get_option(const T& input, Rml::Variant& output) {
    std::string value = "";
    to_json(value, input);

    if (value.empty()) {
        throw std::runtime_error("Invalid value :" + std::to_string(int(input)));
    }

    output = value;
}

template <typename T>
void set_option(T& output, const Rml::Variant& input) {
    T value = T::OptionCount;
    from_json(input.Get<std::string>(), value);

    if (value == T::OptionCount) {
        throw std::runtime_error("Invalid value :" + input.Get<std::string>());
    }

    output = value;
}

template <typename T>
void bind_option(Rml::DataModelConstructor& constructor, const std::string& name, T* option) {
    constructor.BindFunc(name,
        [option](Rml::Variant& out) { get_option(*option, out); },
        [option](const Rml::Variant& in) {
            set_option(*option, in);
            graphics_model_handle.DirtyVariable("options_changed");
            graphics_model_handle.DirtyVariable("ds_info");
        }
    );
};

template <typename T>
void bind_atomic(Rml::DataModelConstructor& constructor, Rml::DataModelHandle handle, const char* name, std::atomic<T>* atomic_val) {
    constructor.BindFunc(name,
        [atomic_val](Rml::Variant& out) {
            out = atomic_val->load();
        },
        [atomic_val, handle, name](const Rml::Variant& in) mutable {
            atomic_val->store(in.Get<T>());
            handle.DirtyVariable(name);
        }
    );
}

static int scanned_binding_index = -1;
static int scanned_input_index = -1;
static int focused_input_index = -1;
static int focused_config_option_index = -1;

static bool msaa2x_supported = false;
static bool msaa4x_supported = false;
static bool msaa8x_supported = false;
static bool sample_positions_supported = false;

static bool cont_active = true;

static dino::input::InputDevice cur_device = dino::input::InputDevice::Controller;

int dino::input::get_scanned_input_index() {
    return scanned_input_index;
}

void dino::input::finish_scanning_input(dino::input::InputField scanned_field) {
    dino::input::set_input_binding(static_cast<dino::input::GameInput>(scanned_input_index), scanned_binding_index, cur_device, scanned_field);
    scanned_input_index = -1;
    scanned_binding_index = -1;
    controls_model_handle.DirtyVariable("inputs");
    controls_model_handle.DirtyVariable("active_binding_input");
    controls_model_handle.DirtyVariable("active_binding_slot");
    nav_help_model_handle.DirtyVariable("nav_help__accept");
    nav_help_model_handle.DirtyVariable("nav_help__exit");
    graphics_model_handle.DirtyVariable("gfx_help__apply");
}

void dino::input::cancel_scanning_input() {
    dino::input::stop_scanning_input();
    scanned_input_index = -1;
    scanned_binding_index = -1;
    controls_model_handle.DirtyVariable("inputs");
    controls_model_handle.DirtyVariable("active_binding_input");
    controls_model_handle.DirtyVariable("active_binding_slot");
    nav_help_model_handle.DirtyVariable("nav_help__accept");
    nav_help_model_handle.DirtyVariable("nav_help__exit");
    graphics_model_handle.DirtyVariable("gfx_help__apply");
}

void dino::input::config_menu_set_cont_or_kb(bool cont_interacted) {
    if (cont_active != cont_interacted) {
        cont_active = cont_interacted;

        if (nav_help_model_handle) {
            nav_help_model_handle.DirtyVariable("nav_help__navigate");
            nav_help_model_handle.DirtyVariable("nav_help__accept");
            nav_help_model_handle.DirtyVariable("nav_help__exit");
        }

        if (graphics_model_handle) {
            graphics_model_handle.DirtyVariable("gfx_help__apply");
        }
    }
}

void close_config_menu_impl() {
    dino::config::save_config();

    recompui::ContextId config_context = recompui::get_config_context_id();
    recompui::ContextId sub_menu_context = recompui::get_config_sub_menu_context_id();

    if (recompui::is_context_shown(sub_menu_context)) {
    	recompui::hide_context(sub_menu_context);
    }
    else {
    	recompui::hide_context(config_context);
    }

    if (!ultramodern::is_game_started()) {
        recompui::show_context(recompui::get_launcher_context_id(), "");
    }
}

void apply_graphics_config(void) {
    ultramodern::renderer::set_graphics_config(new_options);
#if defined(__linux__) // TODO: Remove once RT64 gets native fullscreen support on Linux
    if (new_options.wm_option == ultramodern::renderer::WindowMode::Fullscreen) {
        SDL_SetWindowFullscreen(dino::runtime::get_window(),SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(dino::runtime::get_window(),0);
    }
#endif
}

void close_config_menu() {
    if (ultramodern::renderer::get_graphics_config() != new_options) {
        recompui::open_choice_prompt(
            "Graphics options have changed",
            "Would you like to apply or discard the changes?",
            "Apply",
            "Discard",
            []() {
                apply_graphics_config();
                graphics_model_handle.DirtyAllVariables();
                close_config_menu_impl();
            },
            []() {
                new_options = ultramodern::renderer::get_graphics_config();
                graphics_model_handle.DirtyAllVariables();
                close_config_menu_impl();
            },
            recompui::ButtonVariant::Success,
            recompui::ButtonVariant::Error,
            true,
            "config__close-menu-button"
        );
        return;
    }

    close_config_menu_impl();
}

void dino::config::open_quit_game_prompt() {
    recompui::open_choice_prompt(
        "Are you sure you want to quit?",
        "Any progress since your last save will be lost.",
        "Quit",
        "Cancel",
        []() {
            ultramodern::quit();
        },
        []() {},
        recompui::ButtonVariant::Error,
        recompui::ButtonVariant::Tertiary,
        true,
        "config__quit-game-button"
    );
}

// These defaults values don't matter, as the config file handling overrides them.
struct ControlOptionsContext {
    int rumble_strength; // 0 to 100
    int gyro_sensitivity; // 0 to 100
    int mouse_sensitivity; // 0 to 100
    int joystick_deadzone; // 0 to 100
    int joystick_range; // 0 to 100
    dino::config::TargetingMode targeting_mode;
    dino::input::BackgroundInputMode background_input_mode;
    dino::config::AutosaveMode autosave_mode;
    dino::config::CameraInvertMode camera_invert_mode;
    dino::config::AnalogCamMode analog_cam_mode;
    dino::config::CameraInvertMode analog_camera_invert_mode;
	std::atomic<int> sixty_fps_enabled = 0;
};

ControlOptionsContext control_options_context;

int dino::input::get_rumble_strength() {
    return control_options_context.rumble_strength;
}

void dino::input::set_rumble_strength(int strength) {
    control_options_context.rumble_strength = strength;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("rumble_strength");
    }
}

int dino::input::get_gyro_sensitivity() {
    return control_options_context.gyro_sensitivity;
}

int dino::input::get_mouse_sensitivity() {
    return control_options_context.mouse_sensitivity;
}

int dino::input::get_joystick_deadzone() {
    return control_options_context.joystick_deadzone;
}

int dino::input::get_joystick_range() {
    return control_options_context.joystick_range;
}

void dino::input::set_gyro_sensitivity(int sensitivity) {
    control_options_context.gyro_sensitivity = sensitivity;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("gyro_sensitivity");
    }
}

void dino::input::set_mouse_sensitivity(int sensitivity) {
    control_options_context.mouse_sensitivity = sensitivity;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("mouse_sensitivity");
    }
}

void dino::input::set_joystick_deadzone(int deadzone) {
    control_options_context.joystick_deadzone = deadzone;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("joystick_deadzone");
    }
}

void dino::input::set_joystick_range(int range) {
    control_options_context.joystick_range = range;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("joystick_range");
    }
}

dino::config::TargetingMode dino::config::get_targeting_mode() {
    return control_options_context.targeting_mode;
}

void dino::config::set_targeting_mode(dino::config::TargetingMode mode) {
    control_options_context.targeting_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("targeting_mode");
    }
}

dino::input::BackgroundInputMode dino::input::get_background_input_mode() {
    return control_options_context.background_input_mode;
}

void dino::input::set_background_input_mode(dino::input::BackgroundInputMode mode) {
    control_options_context.background_input_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("background_input_mode");
    }
    SDL_SetHint(
        SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
        mode == dino::input::BackgroundInputMode::On
            ? "1"
            : "0"
    );
}

dino::config::AutosaveMode dino::config::get_autosave_mode() {
    return control_options_context.autosave_mode;
}

void dino::config::set_autosave_mode(dino::config::AutosaveMode mode) {
    control_options_context.autosave_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("autosave_mode");
    }
}

dino::config::CameraInvertMode dino::config::get_camera_invert_mode() {
    return control_options_context.camera_invert_mode;
}

void dino::config::set_camera_invert_mode(dino::config::CameraInvertMode mode) {
    control_options_context.camera_invert_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("camera_invert_mode");
    }
}

dino::config::AnalogCamMode dino::config::get_analog_cam_mode() {
    return control_options_context.analog_cam_mode;
}

void dino::config::set_analog_cam_mode(dino::config::AnalogCamMode mode) {
    control_options_context.analog_cam_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("analog_cam_mode");
    }
}

dino::config::CameraInvertMode dino::config::get_analog_camera_invert_mode() {
    return control_options_context.analog_camera_invert_mode;
}

void dino::config::set_analog_camera_invert_mode(dino::config::CameraInvertMode mode) {
    control_options_context.analog_camera_invert_mode = mode;
    if (general_model_handle) {
        general_model_handle.DirtyVariable("analog_camera_invert_mode");
    }
}

bool dino::config::get_sixty_fps_enabled() {
	return (bool)control_options_context.sixty_fps_enabled.load();
}

void dino::config::set_sixty_fps_enabled(bool enabled) {
	control_options_context.sixty_fps_enabled.store((int)enabled);
	if (general_model_handle) {
		general_model_handle.DirtyVariable("sixty_fps");
	}
}

struct SoundOptionsContext {
    std::atomic<int> main_volume; // Option to control the volume of all sound
    std::atomic<int> bgm_volume;
    std::atomic<int> sfx_volume;
    std::atomic<int> dialog_volume;
	std::atomic<int> subtitles = 1;

    void reset() {
        main_volume = 100;
        bgm_volume = 100;
        sfx_volume = 100;
        dialog_volume = 100;
        subtitles = 1;
    }
    SoundOptionsContext() {
        reset();
    }
};

SoundOptionsContext sound_options_context;

void dino::config::reset_sound_settings() {
    sound_options_context.reset();
    if (sound_options_model_handle) {
        sound_options_model_handle.DirtyAllVariables();
    }
}

void dino::config::set_main_volume(int volume) {
    sound_options_context.main_volume.store(volume);
    if (sound_options_model_handle) {
        sound_options_model_handle.DirtyVariable("main_volume");
    }
}

int dino::config::get_main_volume() {
    return sound_options_context.main_volume.load();
}

void dino::config::set_bgm_volume(int volume) {
    sound_options_context.bgm_volume.store(volume);
    if (sound_options_model_handle) {
        sound_options_model_handle.DirtyVariable("bgm_volume");
    }
}

int dino::config::get_bgm_volume() {
    return sound_options_context.bgm_volume.load();
}

void dino::config::set_sfx_volume(int volume) {
    sound_options_context.sfx_volume.store(volume);
    if (sound_options_model_handle) {
        sound_options_model_handle.DirtyVariable("sfx_volume");
    }
}

int dino::config::get_sfx_volume() {
    return sound_options_context.sfx_volume.load();
}

void dino::config::set_dialog_volume(int volume) {
    sound_options_context.dialog_volume.store(volume);
    if (sound_options_model_handle) {
        sound_options_model_handle.DirtyVariable("dialog_volume");
    }
}

int dino::config::get_dialog_volume() {
    return sound_options_context.dialog_volume.load();
}

bool dino::config::get_subtitles_enabled() {
	return (bool)sound_options_context.subtitles.load();
}

void dino::config::set_subtitles_enabled(bool enabled) {
	sound_options_context.subtitles.store((int)enabled);
	if (general_model_handle) {
		general_model_handle.DirtyVariable("subtitles");
	}
}

struct DebugContext {
    Rml::DataModelHandle model_handle;
	std::atomic<int> debug_ui_enabled = 1;
	std::atomic<int> debug_stdout_enabled = 0;
	std::atomic<int> debug_dll_logging_enabled = 0;
	std::atomic<int> debug_diprintf_enabled = 0;
};

DebugContext debug_context;

recompui::ContextId config_context;

recompui::ContextId recompui::get_config_context_id() {
	return config_context;
}

// Helper copied from RmlUi to get a named child.
Rml::Element* recompui::get_child_by_tag(Rml::Element* parent, const std::string& tag)
{
	// Look for the existing child
	for (int i = 0; i < parent->GetNumChildren(); i++)
	{
        Rml::Element* child = parent->GetChild(i);
		if (child->GetTagName() == tag)
			return child;
	}

    return nullptr;
}

class ConfigTabsetListener : public Rml::EventListener {
    void ProcessEvent(Rml::Event& event) override {
        if (event.GetId() == Rml::EventId::Tabchange) {
            int tab_index = event.GetParameter<int>("tab_index", 0);
            bool in_mod_tab = (tab_index == recompui::config_tab_to_index(recompui::ConfigTab::Mods));
            if (in_mod_tab) {
                recompui::set_config_tabset_mod_nav();
            }
            else {
                Rml::ElementTabSet* tabset = recompui::get_config_tabset();
                Rml::Element* tabs = recompui::get_child_by_tag(tabset, "tabs");
                if (tabs != nullptr) {
                    size_t num_children = tabs->GetNumChildren();
                    for (size_t i = 0; i < num_children; i++) {
                        tabs->GetChild(i)->SetProperty(Rml::PropertyId::NavDown, Rml::Style::Nav::Auto);
                    }
                }
            }
        }
    }
};

class ConfigMenu : public recompui::MenuController {
private:
    ConfigTabsetListener config_tabset_listener;
public:
    ConfigMenu() {

    }
    ~ConfigMenu() override {

    }
    void load_document() override {
        config_context = recompui::create_context(dino::runtime::get_asset_path("config_menu.rml"));
        recompui::update_mod_list(false);
        recompui::get_config_tabset()->AddEventListener(Rml::EventId::Tabchange, &config_tabset_listener);
    }
    void register_events(recompui::UiEventListenerInstancer& listener) override {
        recompui::register_event(listener, "apply_options",
            [](const std::string& param, Rml::Event& event) {
                graphics_model_handle.DirtyVariable("options_changed");
                apply_graphics_config();
            });
        recompui::register_event(listener, "config_keydown",
            [](const std::string& param, Rml::Event& event) {
                if (!recompui::is_prompt_open() && event.GetId() == Rml::EventId::Keydown) {
                    auto key = event.GetParameter<Rml::Input::KeyIdentifier>("key_identifier", Rml::Input::KeyIdentifier::KI_UNKNOWN);
                    switch (key) {
                        case Rml::Input::KeyIdentifier::KI_ESCAPE:
                            close_config_menu();
                            break;
                        case Rml::Input::KeyIdentifier::KI_F:
                            graphics_model_handle.DirtyVariable("options_changed");
                            apply_graphics_config();
                            break;
                    }
                }
            });
        // This needs to be separate from `close_config_menu` so it ensures that the event is only on the target
        recompui::register_event(listener, "close_config_menu_backdrop",
            [](const std::string& param, Rml::Event& event) {
                if (event.GetPhase() == Rml::EventPhase::Target) {
                    close_config_menu();
                }
            });
        recompui::register_event(listener, "close_config_menu",
            [](const std::string& param, Rml::Event& event) {
                close_config_menu();
            });

        recompui::register_event(listener, "open_quit_game_prompt",
            [](const std::string& param, Rml::Event& event) {
                dino::config::open_quit_game_prompt();
            });

        recompui::register_event(listener, "toggle_input_device",
            [](const std::string& param, Rml::Event& event) {
                cur_device = cur_device == dino::input::InputDevice::Controller
                    ? dino::input::InputDevice::Keyboard
                    : dino::input::InputDevice::Controller;
                controls_model_handle.DirtyVariable("input_device_is_keyboard");
                controls_model_handle.DirtyVariable("inputs");
            });
    }

    void bind_config_list_events(Rml::DataModelConstructor &constructor) {
        constructor.BindEventCallback("set_cur_config_index",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                int option_index = inputs.at(0).Get<size_t>();
                // watch for mouseout being overzealous during event bubbling, only clear if the event's attached element matches the current
                if (option_index == -1 && event.GetType() == "mouseout" && event.GetCurrentElement() != event.GetTargetElement()) {
                    return;
                }
                focused_config_option_index = option_index;
                model_handle.DirtyVariable("cur_config_index");
            });

        constructor.Bind("cur_config_index", &focused_config_option_index);
    }

    void make_graphics_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("graphics_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for the graphics config menu");
        }

        ultramodern::sleep_milliseconds(50);
        new_options = ultramodern::renderer::get_graphics_config();
        bind_config_list_events(constructor);

        constructor.BindFunc("res_option",
            [](Rml::Variant& out) { get_option(new_options.res_option, out); },
            [](const Rml::Variant& in) {
                set_option(new_options.res_option, in);
                graphics_model_handle.DirtyVariable("options_changed");
                graphics_model_handle.DirtyVariable("ds_info");
                graphics_model_handle.DirtyVariable("ds_option");
            }
        );
        bind_option(constructor, "wm_option", &new_options.wm_option);
        bind_option(constructor, "ar_option", &new_options.ar_option);
        bind_option(constructor, "hr_option", &new_options.hr_option);
        bind_option(constructor, "msaa_option", &new_options.msaa_option);
        bind_option(constructor, "rr_option", &new_options.rr_option);
        constructor.BindFunc("rr_manual_value",
            [](Rml::Variant& out) {
                out = new_options.rr_manual_value;
            },
            [](const Rml::Variant& in) {
                new_options.rr_manual_value = in.Get<int>();
                graphics_model_handle.DirtyVariable("options_changed");
            });
        constructor.BindFunc("ds_option",
            [](Rml::Variant& out) {
                if (new_options.res_option == ultramodern::renderer::Resolution::Auto) {
                    out = 1;
                } else {
                    out = new_options.ds_option;
                }
            },
            [](const Rml::Variant& in) {
                new_options.ds_option = in.Get<int>();
                graphics_model_handle.DirtyVariable("options_changed");
                graphics_model_handle.DirtyVariable("ds_info");
            });

        constructor.BindFunc("display_refresh_rate",
            [](Rml::Variant& out) {
                out = ultramodern::get_display_refresh_rate();
            });

        constructor.BindFunc("options_changed",
            [](Rml::Variant& out) {
                out = (ultramodern::renderer::get_graphics_config() != new_options);
            });
        constructor.BindFunc("ds_info",
            [](Rml::Variant& out) {
                switch (new_options.res_option) {
                    default:
                    case ultramodern::renderer::Resolution::Auto:
                        out = "Downsampling is not available at auto resolution";
                        return;
                    case ultramodern::renderer::Resolution::Original:
                        if (new_options.ds_option == 2) {
                            out = "Rendered in 480p and scaled to 240p";
                        } else if (new_options.ds_option == 4) {
                            out = "Rendered in 960p and scaled to 240p";
                        }
                        return;
                    case ultramodern::renderer::Resolution::Original2x:
                        if (new_options.ds_option == 2) {
                            out = "Rendered in 960p and scaled to 480p";
                        } else if (new_options.ds_option == 4) {
                            out = "Rendered in 4K and scaled to 480p";
                        }
                        return;
                }
                out = "";
            });
        
        constructor.BindFunc("gfx_help__apply", [](Rml::Variant& out) {
            if (cont_active) {
                out = \
                    (dino::input::get_input_binding(dino::input::GameInput::APPLY_MENU, 0, dino::input::InputDevice::Controller).to_string() != "" ?
                        " " + dino::input::get_input_binding(dino::input::GameInput::APPLY_MENU, 0, dino::input::InputDevice::Controller).to_string() :
                        ""
                    ) + \
                    (dino::input::get_input_binding(dino::input::GameInput::APPLY_MENU, 1, dino::input::InputDevice::Controller).to_string() != "" ?
                        " " + dino::input::get_input_binding(dino::input::GameInput::APPLY_MENU, 1, dino::input::InputDevice::Controller).to_string() :
                        ""
                    );
            } else {
                out = " " PF_KEYBOARD_F;
            }
        });

        constructor.Bind("msaa2x_supported", &msaa2x_supported);
        constructor.Bind("msaa4x_supported", &msaa4x_supported);
        constructor.Bind("msaa8x_supported", &msaa8x_supported);
        constructor.Bind("sample_positions_supported", &sample_positions_supported);

        graphics_model_handle = constructor.GetModelHandle();
    }

    void make_controls_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("controls_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for the controls config menu");
        }

        constructor.BindFunc("input_count", [](Rml::Variant& out) { out = static_cast<uint64_t>(dino::input::get_num_inputs()); } );
        constructor.BindFunc("input_device_is_keyboard", [](Rml::Variant& out) { out = cur_device == dino::input::InputDevice::Keyboard; } );

        constructor.RegisterTransformFunc("get_input_name", [](const Rml::VariantList& inputs) {
            return Rml::Variant{dino::input::get_input_name(static_cast<dino::input::GameInput>(inputs.at(0).Get<size_t>()))};
        });

        constructor.RegisterTransformFunc("get_input_enum_name", [](const Rml::VariantList& inputs) {
            return Rml::Variant{dino::input::get_input_enum_name(static_cast<dino::input::GameInput>(inputs.at(0).Get<size_t>()))};
        });

        constructor.BindEventCallback("set_input_binding",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                scanned_input_index = inputs.at(0).Get<size_t>();
                scanned_binding_index = inputs.at(1).Get<size_t>();
                dino::input::start_scanning_input(cur_device);
                model_handle.DirtyVariable("active_binding_input");
                model_handle.DirtyVariable("active_binding_slot");
            });

        constructor.BindEventCallback("reset_input_bindings_to_defaults",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                if (cur_device == dino::input::InputDevice::Controller) {
                    dino::config::reset_cont_input_bindings();
                } else {
                    dino::config::reset_kb_input_bindings();
                }
                model_handle.DirtyAllVariables();
                nav_help_model_handle.DirtyVariable("nav_help__accept");
                nav_help_model_handle.DirtyVariable("nav_help__exit");
                graphics_model_handle.DirtyVariable("gfx_help__apply");
            });

        constructor.BindEventCallback("clear_input_bindings",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                dino::input::GameInput input = static_cast<dino::input::GameInput>(inputs.at(0).Get<size_t>());
                for (size_t binding_index = 0; binding_index < dino::input::bindings_per_input; binding_index++) {
                    dino::input::set_input_binding(input, binding_index, cur_device, dino::input::InputField{});
                }
                model_handle.DirtyVariable("inputs");
                graphics_model_handle.DirtyVariable("gfx_help__apply");
            });

        constructor.BindEventCallback("reset_single_input_binding_to_default",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                dino::input::GameInput input = static_cast<dino::input::GameInput>(inputs.at(0).Get<size_t>());
                dino::config::reset_single_input_binding(cur_device, input);
                model_handle.DirtyVariable("inputs");
                nav_help_model_handle.DirtyVariable("nav_help__accept");
                nav_help_model_handle.DirtyVariable("nav_help__exit");
            });

        constructor.BindEventCallback("set_input_row_focus",
            [](Rml::DataModelHandle model_handle, Rml::Event& event, const Rml::VariantList& inputs) {
                int input_index = inputs.at(0).Get<size_t>();
                // watch for mouseout being overzealous during event bubbling, only clear if the event's attached element matches the current
                if (input_index == -1 && event.GetType() == "mouseout" && event.GetCurrentElement() != event.GetTargetElement()) {
                    return;
                }
                focused_input_index = input_index;
                model_handle.DirtyVariable("cur_input_row");
            });

        // Rml variable definition for an individual InputField.
        struct InputFieldVariableDefinition : public Rml::VariableDefinition {
            InputFieldVariableDefinition() : Rml::VariableDefinition(Rml::DataVariableType::Scalar) {}

            virtual bool Get(void* ptr, Rml::Variant& variant) override { variant = reinterpret_cast<dino::input::InputField*>(ptr)->to_string(); return true; }
            virtual bool Set(void* ptr, const Rml::Variant& variant) override { return false; }
        };
        // Static instance of the InputField variable definition to have a pointer to return to RmlUi.
        static InputFieldVariableDefinition input_field_definition_instance{};

        // Rml variable definition for an array of InputField values (e.g. all the bindings for a single input).
        struct BindingContainerVariableDefinition : public Rml::VariableDefinition {
            BindingContainerVariableDefinition() : Rml::VariableDefinition(Rml::DataVariableType::Array) {}

            virtual bool Get(void* ptr, Rml::Variant& variant) override { return false; }
            virtual bool Set(void* ptr, const Rml::Variant& variant) override { return false; }

            virtual int Size(void* ptr) override { return dino::input::bindings_per_input; }
            virtual Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override {
                dino::input::GameInput input = static_cast<dino::input::GameInput>((uintptr_t)ptr);
                return Rml::DataVariable{&input_field_definition_instance, &dino::input::get_input_binding(input, address.index, cur_device)};
            }
        };
        // Static instance of the InputField array variable definition to have a fixed pointer to return to RmlUi.
        static BindingContainerVariableDefinition binding_container_var_instance{};

        // Rml variable definition for an array of an array of InputField values (e.g. all the bindings for all inputs).
        struct BindingArrayContainerVariableDefinition : public Rml::VariableDefinition {
            BindingArrayContainerVariableDefinition() : Rml::VariableDefinition(Rml::DataVariableType::Array) {}

            virtual bool Get(void* ptr, Rml::Variant& variant) override { return false; }
            virtual bool Set(void* ptr, const Rml::Variant& variant) override { return false; }

            virtual int Size(void* ptr) override { return dino::input::get_num_inputs(); }
            virtual Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override {
                // Encode the input index as the pointer to avoid needing to do any allocations.
                return Rml::DataVariable(&binding_container_var_instance, (void*)(uintptr_t)address.index);
            }
        };

        // Static instance of the BindingArrayContainerVariableDefinition variable definition to have a fixed pointer to return to RmlUi.
        static BindingArrayContainerVariableDefinition binding_array_var_instance{};

        struct InputContainerVariableDefinition : public Rml::VariableDefinition {
            InputContainerVariableDefinition() : Rml::VariableDefinition(Rml::DataVariableType::Struct) {}

            virtual bool Get(void* ptr, Rml::Variant& variant) override { return true; }
            virtual bool Set(void* ptr, const Rml::Variant& variant) override { return false; }

            virtual int Size(void* ptr) override { return dino::input::get_num_inputs(); }
            virtual Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override {
                if (address.name == "array") {
                    return Rml::DataVariable(&binding_array_var_instance, nullptr);
                }
                else {
                    dino::input::GameInput input = dino::input::get_input_from_enum_name(address.name);
                    if (input != dino::input::GameInput::COUNT) {
                        return Rml::DataVariable(&binding_container_var_instance, (void*)(uintptr_t)input);
                    }
                }
                return Rml::DataVariable{};
            }
        };

        // Dummy type to associate with the variable definition.
        struct InputContainer {};
        constructor.RegisterCustomDataVariableDefinition<InputContainer>(Rml::MakeUnique<InputContainerVariableDefinition>());

        // Dummy instance of the dummy type to bind to the variable.
        static InputContainer dummy_container;
        constructor.Bind("inputs", &dummy_container);

        constructor.BindFunc("cur_input_row", [](Rml::Variant& out) {
            if (focused_input_index == -1) {
                out = "NONE";
            }
            else {
                out = dino::input::get_input_enum_name(static_cast<dino::input::GameInput>(focused_input_index));
            }
        });

        constructor.BindFunc("active_binding_input", [](Rml::Variant& out) {
            if (scanned_input_index == -1) {
                out = "NONE";
            }
            else {
                out = dino::input::get_input_enum_name(static_cast<dino::input::GameInput>(scanned_input_index));
            }
        });

        constructor.Bind<int>("active_binding_slot", &scanned_binding_index);

        controls_model_handle = constructor.GetModelHandle();
    }

    void make_nav_help_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("nav_help_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for nav help");
        }

        constructor.BindFunc("nav_help__navigate", [](Rml::Variant& out) {
            if (cont_active) {
                out = PF_DPAD;
            } else {
                out = PF_KEYBOARD_ARROWS PF_KEYBOARD_TAB;
            }
        });

        constructor.BindFunc("nav_help__accept", [](Rml::Variant& out) {
            if (cont_active) {
                out = \
                    dino::input::get_input_binding(dino::input::GameInput::ACCEPT_MENU, 0, dino::input::InputDevice::Controller).to_string() + \
                    dino::input::get_input_binding(dino::input::GameInput::ACCEPT_MENU, 1, dino::input::InputDevice::Controller).to_string();
            } else {
                out = PF_KEYBOARD_ENTER;
            }
        });

        constructor.BindFunc("nav_help__exit", [](Rml::Variant& out) {
            if (cont_active) {
                out = \
                    dino::input::get_input_binding(dino::input::GameInput::TOGGLE_MENU, 0, dino::input::InputDevice::Controller).to_string() + \
                    dino::input::get_input_binding(dino::input::GameInput::TOGGLE_MENU, 1, dino::input::InputDevice::Controller).to_string();
            } else {
                out = PF_KEYBOARD_ESCAPE;
            }
        });

        nav_help_model_handle = constructor.GetModelHandle();
    }

    void make_general_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("general_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for the control options menu");
        }

        bind_config_list_events(constructor);

        general_model_handle = constructor.GetModelHandle();
        
        constructor.Bind("rumble_strength", &control_options_context.rumble_strength);
        constructor.Bind("gyro_sensitivity", &control_options_context.gyro_sensitivity);
        constructor.Bind("mouse_sensitivity", &control_options_context.mouse_sensitivity);
        constructor.Bind("joystick_deadzone", &control_options_context.joystick_deadzone);
        constructor.Bind("joystick_range", &control_options_context.joystick_range);
        bind_option(constructor, "targeting_mode", &control_options_context.targeting_mode);
        bind_option(constructor, "background_input_mode", &control_options_context.background_input_mode);
        bind_option(constructor, "autosave_mode", &control_options_context.autosave_mode);
        bind_option(constructor, "camera_invert_mode", &control_options_context.camera_invert_mode);
        bind_option(constructor, "analog_cam_mode", &control_options_context.analog_cam_mode);
        bind_option(constructor, "analog_camera_invert_mode", &control_options_context.analog_camera_invert_mode);
        bind_atomic(constructor, general_model_handle, "sixty_fps", &control_options_context.sixty_fps_enabled);
    }
    
    void make_sound_options_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("sound_options_model");
        if (!constructor) {
            throw std::runtime_error("Failed to make RmlUi data model for the sound options menu");
        }

        bind_config_list_events(constructor);
        
        sound_options_model_handle = constructor.GetModelHandle();

        bind_atomic(constructor, sound_options_model_handle, "main_volume", &sound_options_context.main_volume);
        bind_atomic(constructor, sound_options_model_handle, "bgm_volume", &sound_options_context.bgm_volume);
        bind_atomic(constructor, sound_options_model_handle, "sfx_volume", &sound_options_context.sfx_volume);
        bind_atomic(constructor, sound_options_model_handle, "dialog_volume", &sound_options_context.dialog_volume);
        bind_atomic(constructor, sound_options_model_handle, "subtitles", &sound_options_context.subtitles);
    }

    void make_debug_bindings(Rml::Context* context) {
        Rml::DataModelConstructor constructor = context->CreateDataModel("debug_model");
		if (!constructor) {
			throw std::runtime_error("Failed to make RmlUi data model for the debug menu");
		}

		debug_context.model_handle = constructor.GetModelHandle();

		bind_config_list_events(constructor);

		bind_atomic(constructor, debug_context.model_handle, "debug_ui_enabled", &debug_context.debug_ui_enabled);
		bind_atomic(constructor, debug_context.model_handle, "debug_stdout_enabled", &debug_context.debug_stdout_enabled);
		bind_atomic(constructor, debug_context.model_handle, "debug_dll_logging_enabled", &debug_context.debug_dll_logging_enabled);
		bind_atomic(constructor, debug_context.model_handle, "debug_diprintf_enabled", &debug_context.debug_diprintf_enabled);
		
		// Register the array type for string vectors.
		constructor.RegisterArray<std::vector<std::string>>();
    }

    void make_bindings(Rml::Context* context) override {
        // initially set cont state for ui help
        //recomp::config_menu_set_cont_or_kb(recompui::get_cont_active());
        make_nav_help_bindings(context);
        make_general_bindings(context);
        make_controls_bindings(context);
        make_graphics_bindings(context);
        make_sound_options_bindings(context);
        make_debug_bindings(context);
    }
};

std::unique_ptr<recompui::MenuController> recompui::create_config_menu() {
    return std::make_unique<ConfigMenu>();
}

bool dino::config::get_debug_ui_enabled() {
	return (bool)debug_context.debug_ui_enabled.load();
}

void dino::config::set_debug_ui_enabled(bool enabled) {
	debug_context.debug_ui_enabled.store((int)enabled);
	if (debug_context.model_handle) {
		debug_context.model_handle.DirtyVariable("debug_ui_enabled");
	}
}

bool dino::config::get_debug_stdout_enabled() {
	return (bool)debug_context.debug_stdout_enabled.load();
}

void dino::config::set_debug_stdout_enabled(bool enabled) {
	debug_context.debug_stdout_enabled.store((int)enabled);
	if (debug_context.model_handle) {
		debug_context.model_handle.DirtyVariable("debug_stdout_enabled");
	}
}

bool dino::config::get_debug_dll_logging_enabled() {
	return (bool)debug_context.debug_dll_logging_enabled.load();
}

void dino::config::set_debug_dll_logging_enabled(bool enabled) {
	debug_context.debug_dll_logging_enabled.store((int)enabled);
	if (debug_context.model_handle) {
		debug_context.model_handle.DirtyVariable("debug_dll_logging_enabled");
	}
}

bool dino::config::get_debug_diprintf_enabled() {
	return (bool)debug_context.debug_diprintf_enabled.load();
}

void dino::config::set_debug_diprintf_enabled(bool enabled) {
	debug_context.debug_diprintf_enabled.store((int)enabled);
	if (debug_context.model_handle) {
		debug_context.model_handle.DirtyVariable("debug_diprintf_enabled");
	}
}

void recompui::update_supported_options() {
    msaa2x_supported = dino::renderer::RT64MaxMSAA() >= RT64::UserConfiguration::Antialiasing::MSAA2X;
    msaa4x_supported = dino::renderer::RT64MaxMSAA() >= RT64::UserConfiguration::Antialiasing::MSAA4X;
    msaa8x_supported = dino::renderer::RT64MaxMSAA() >= RT64::UserConfiguration::Antialiasing::MSAA8X;
    sample_positions_supported = dino::renderer::RT64SamplePositionsSupported();
    
    new_options = ultramodern::renderer::get_graphics_config();

    graphics_model_handle.DirtyAllVariables();
}

void recompui::toggle_fullscreen() {
    new_options.wm_option = (new_options.wm_option == ultramodern::renderer::WindowMode::Windowed) ? ultramodern::renderer::WindowMode::Fullscreen : ultramodern::renderer::WindowMode::Windowed;
    apply_graphics_config();
    graphics_model_handle.DirtyVariable("wm_option");
}

void recompui::set_config_tab(ConfigTab tab) {
    get_config_tabset()->SetActiveTab(config_tab_to_index(tab));
}

Rml::ElementTabSet* recompui::get_config_tabset() {
    ContextId config_context = recompui::get_config_context_id();

    ContextId old_context = recompui::try_close_current_context();

    Rml::ElementDocument *doc = config_context.get_document();
    assert(doc != nullptr);

    Rml::Element *tabset_el = doc->GetElementById("config_tabset");
    assert(tabset_el != nullptr);

    Rml::ElementTabSet *tabset = rmlui_dynamic_cast<Rml::ElementTabSet *>(tabset_el);
    assert(tabset != nullptr);

    if (old_context != ContextId::null()) {
        old_context.open();
    }

    return tabset;
}

Rml::Element* recompui::get_mod_tab() {
    ContextId config_context = recompui::get_config_context_id();

    ContextId old_context = recompui::try_close_current_context();

    Rml::ElementDocument* doc = config_context.get_document();
    assert(doc != nullptr);

    Rml::Element* tab_el = doc->GetElementById("tab_mods");
    assert(tab_el != nullptr);

    if (old_context != ContextId::null()) {
        old_context.open();
    }

    return tab_el;
}
