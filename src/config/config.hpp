#pragma once

#include <filesystem>
#include <string_view>

#include "json/json.hpp"

#include "input/input.hpp"

namespace dino::config {
    constexpr std::u8string_view program_id = u8"DinoPlanetRecompiled";
    constexpr std::string_view program_name = "Dinosaur Planet: Recompiled";

    void load_config();
    void save_config();
    
    void reset_input_bindings();
    void reset_cont_input_bindings();
    void reset_kb_input_bindings();
    void reset_single_input_binding(dino::input::InputDevice device, dino::input::GameInput input);

    std::filesystem::path get_app_folder_path();
    
    enum class AutosaveMode {
        On,
        Off,
        OptionCount
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(dino::config::AutosaveMode, {
        {dino::config::AutosaveMode::On, "On"},
        {dino::config::AutosaveMode::Off, "Off"}
    });

    enum class TargetingMode {
        Switch,
        Hold,
        OptionCount
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(dino::config::TargetingMode, {
        {dino::config::TargetingMode::Switch, "Switch"},
        {dino::config::TargetingMode::Hold, "Hold"}
    });

    TargetingMode get_targeting_mode();
    void set_targeting_mode(TargetingMode mode);

    enum class CameraInvertMode {
        InvertNone,
        InvertX,
        InvertY,
        InvertBoth,
        OptionCount
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(dino::config::CameraInvertMode, {
        {dino::config::CameraInvertMode::InvertNone, "InvertNone"},
        {dino::config::CameraInvertMode::InvertX, "InvertX"},
        {dino::config::CameraInvertMode::InvertY, "InvertY"},
        {dino::config::CameraInvertMode::InvertBoth, "InvertBoth"}
    });

    CameraInvertMode get_camera_invert_mode();
    void set_camera_invert_mode(CameraInvertMode mode);

    CameraInvertMode get_analog_camera_invert_mode();
    void set_analog_camera_invert_mode(CameraInvertMode mode);

    enum class AnalogCamMode {
        On,
        Off,
		OptionCount
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(dino::config::AnalogCamMode, {
        {dino::config::AnalogCamMode::On, "On"},
        {dino::config::AnalogCamMode::Off, "Off"}
    });

    AutosaveMode get_autosave_mode();
    void set_autosave_mode(AutosaveMode mode);

    AnalogCamMode get_analog_cam_mode();
    void set_analog_cam_mode(AnalogCamMode mode);

    bool get_sixty_fps_enabled();
    void set_sixty_fps_enabled(bool enabled);

    void reset_sound_settings();
    void set_main_volume(int volume);
    int get_main_volume();
    void set_bgm_volume(int volume);
    int get_bgm_volume();
    void set_sfx_volume(int volume);
    int get_sfx_volume();
    void set_dialog_volume(int volume);
    int get_dialog_volume();
    bool get_subtitles_enabled();
    void set_subtitles_enabled(bool enabled);

    void open_quit_game_prompt();

    bool get_debug_ui_enabled();
    void set_debug_ui_enabled(bool enabled);

    bool get_debug_stdout_enabled();
    void set_debug_stdout_enabled(bool enabled);

    bool get_debug_dll_logging_enabled();
    void set_debug_dll_logging_enabled(bool enabled);

    bool get_debug_diprintf_enabled();
    void set_debug_diprintf_enabled(bool enabled);
}