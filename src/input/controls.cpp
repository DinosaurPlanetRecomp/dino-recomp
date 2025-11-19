#include "controls.hpp"

#include <array>

namespace dino::input {

// Arrays that hold the mappings for every input for keyboard and controller respectively.
using input_mapping = std::array<InputField, bindings_per_input>;
using input_mapping_array = std::array<input_mapping, static_cast<size_t>(GameInput::COUNT)>;
static input_mapping_array keyboard_input_mappings{};
static input_mapping_array controller_input_mappings{};

// Make the button value array, which maps a button index to its bit field.
#define DEFINE_INPUT(name, value, readable) uint16_t(value##u),
static const std::array n64_button_values = {
    DEFINE_N64_BUTTON_INPUTS()
};
#undef DEFINE_INPUT

// Make the input name array.
#define DEFINE_INPUT(name, value, readable) readable,
static const std::vector<std::string> input_names = {
    DEFINE_ALL_INPUTS()
};
#undef DEFINE_INPUT

// Make the input enum name array.
#define DEFINE_INPUT(name, value, readable) #name,
static const std::vector<std::string> input_enum_names = {
    DEFINE_ALL_INPUTS()
};
#undef DEFINE_INPUT

size_t get_num_inputs() {
    return (size_t)GameInput::COUNT;
}

const std::string& get_input_name(GameInput input) {
    return input_names.at(static_cast<size_t>(input));
}

const std::string& get_input_enum_name(GameInput input) {
    return input_enum_names.at(static_cast<size_t>(input));
}

GameInput get_input_from_enum_name(const std::string_view enum_name) {
    auto find_it = std::find(input_enum_names.begin(), input_enum_names.end(), enum_name);
    if (find_it == input_enum_names.end()) {
        return GameInput::COUNT;
    }

    return static_cast<GameInput>(find_it - input_enum_names.begin());
}

// Due to an RmlUi limitation this can't be const. Ideally it would return a const reference or even just a straight up copy.
InputField& get_input_binding(GameInput input, size_t binding_index, InputDevice device) {
    input_mapping_array& device_mappings = (device == InputDevice::Controller) ?  controller_input_mappings : keyboard_input_mappings;
    input_mapping& cur_input_mapping = device_mappings.at(static_cast<size_t>(input));

    if (binding_index < cur_input_mapping.size()) {
        return cur_input_mapping[binding_index];
    }
    else {
        static InputField dummy_field = {};
        return dummy_field;
    }
}

void set_input_binding(GameInput input, size_t binding_index, InputDevice device, InputField value) {
    input_mapping_array& device_mappings = (device == InputDevice::Controller) ?  controller_input_mappings : keyboard_input_mappings;
    input_mapping& cur_input_mapping = device_mappings.at(static_cast<size_t>(input));

    if (binding_index < cur_input_mapping.size()) {
        cur_input_mapping[binding_index] = value;
    }
}

bool get_n64_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out) {
    uint16_t cur_buttons = 0;
    float cur_x = 0.0f;
    float cur_y = 0.0f;

    if (controller_num != 0) {
        return false;
    }

    if (!game_input_disabled()) {
        for (size_t i = 0; i < n64_button_values.size(); i++) {
            size_t input_index = (size_t)GameInput::N64_BUTTON_START + i;
            cur_buttons |= get_input_digital(keyboard_input_mappings[input_index]) ? n64_button_values[i] : 0;
            cur_buttons |= get_input_digital(controller_input_mappings[input_index]) ? n64_button_values[i] : 0;
        }

        float joystick_x = get_input_analog(controller_input_mappings[(size_t)GameInput::X_AXIS_POS])
                        - get_input_analog(controller_input_mappings[(size_t)GameInput::X_AXIS_NEG]);

        float joystick_y = get_input_analog(controller_input_mappings[(size_t)GameInput::Y_AXIS_POS])
                        - get_input_analog(controller_input_mappings[(size_t)GameInput::Y_AXIS_NEG]);

        apply_joystick_deadzone(joystick_x, joystick_y, &joystick_x, &joystick_y);

        cur_x = get_input_analog(keyboard_input_mappings[(size_t)GameInput::X_AXIS_POS])
                - get_input_analog(keyboard_input_mappings[(size_t)GameInput::X_AXIS_NEG]) + joystick_x;

        cur_y = get_input_analog(keyboard_input_mappings[(size_t)GameInput::Y_AXIS_POS])
                - get_input_analog(keyboard_input_mappings[(size_t)GameInput::Y_AXIS_NEG]) + joystick_y;

        float joystick_range = get_joystick_range() / 100.0f;
        
        cur_x *= joystick_range;
        cur_y *= joystick_range;
    }

    *buttons_out = cur_buttons;
    *x_out = std::clamp(cur_x, -1.0f, 1.0f);
    *y_out = std::clamp(cur_y, -1.0f, 1.0f);

    return true;
}

}
