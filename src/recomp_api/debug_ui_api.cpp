#include "debug_ui_api.hpp"
#include "common.hpp"

#include <vector>

#include "ultramodern/ultramodern.hpp"
#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "imgui.h"

#include "debug_ui/debug_ui.hpp"
#include "config/config.hpp"
#include "common/recomp_helpers.hpp"

extern "C" void dbgui_is_open(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, dino::debug_ui::is_open());
}

extern "C" void dbgui_is_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, dino::config::get_debug_ui_enabled());
}

extern "C" void dbgui_ui_frame_begin(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::ui_frame_begin();
}

extern "C" void dbgui_ui_frame_end(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::ui_frame_end();
}

extern "C" void dbgui_begin(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) name_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(s32) open_ptr = _arg<1, PTR(s32)>(rdram, ctx);

    char *name = dino::recomp_api::copy_rdram_str(name_ptr, rdram, ctx);

    bool expanded = false;
    if ((gpr)open_ptr != 0) {
        bool open = MEM_W(0, (gpr)open_ptr) != 0;
        expanded = dino::debug_ui::begin(name, &open);

        MEM_W(0, (gpr)open_ptr) = open;
    } else {
        expanded = dino::debug_ui::begin(name, nullptr);
    }

    free(name);

    _return<s32>(ctx, expanded);
}

extern "C" void dbgui_end(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::end();
}

extern "C" void dbgui_text(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) text_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *text = dino::recomp_api::copy_rdram_str(text_ptr, rdram, ctx);

    dino::debug_ui::text(text);

    free(text);
}

extern "C" void dbgui_label_text(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(char) text_ptr = _arg<1, PTR(char)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);
    char *text = dino::recomp_api::copy_rdram_str(text_ptr, rdram, ctx);

    dino::debug_ui::label_text(label, text);

    free(label);
    free(text);
}

extern "C" void dbgui_same_line(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::same_line();
}

extern "C" void dbgui_new_line(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::new_line();
}

extern "C" void dbgui_separator(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::separator();
}

extern "C" void dbgui_begin_combo(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(char) preview_ptr = _arg<1, PTR(char)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);
    char *preview = dino::recomp_api::copy_rdram_str(preview_ptr, rdram, ctx);

    bool open = dino::debug_ui::begin_combo(label, preview);

    free(label);
    free(preview);

    _return<s32>(ctx, open);
}

extern "C" void dbgui_end_combo(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::end_combo();
}

extern "C" void dbgui_selectable(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    bool selected = _arg<1, s32>(rdram, ctx) != 0;

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    bool pressed = dino::debug_ui::selectable(label, &selected);

    free(label);

    _return<s32>(ctx, pressed);
}

extern "C" void dbgui_button(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    bool pressed = dino::debug_ui::button(label);

    free(label);

    _return<s32>(ctx, pressed);
}

extern "C" void dbgui_begin_main_menu_bar(uint8_t* rdram, recomp_context* ctx) {
    bool expanded = expanded = dino::debug_ui::begin_main_menu_bar();

    _return<s32>(ctx, expanded);
}

extern "C" void dbgui_end_main_menu_bar(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::end_main_menu_bar();
}

extern "C" void dbgui_begin_menu(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    bool open = dino::debug_ui::begin_menu(label);

    free(label);

    _return<s32>(ctx, open);
}

extern "C" void dbgui_end_menu(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::end_menu();
}

extern "C" void dbgui_menu_item(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(s32) selected_ptr = _arg<1, PTR(s32)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    bool pressed = false;
    if ((gpr)selected_ptr != 0) {
        bool selected = MEM_W(0, (gpr)selected_ptr) != 0;
        pressed = dino::debug_ui::menu_item(label, &selected);

        MEM_W(0, (gpr)selected_ptr) = selected;
    } else {
        pressed = dino::debug_ui::menu_item(label, nullptr);
    }

    free(label);

    _return<s32>(ctx, pressed);
}

extern "C" void dbgui_collapsing_header(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    bool open = dino::debug_ui::collapsing_header(label);

    free(label);

    _return<s32>(ctx, open);
}

extern "C" void dbgui_tree_node(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    bool open = dino::debug_ui::tree_node(label);

    free(label);

    _return<s32>(ctx, open);
}

extern "C" void dbgui_tree_pop(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::tree_pop();
}

extern "C" void dbgui_begin_child(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) str_id_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *str_id = dino::recomp_api::copy_rdram_str(str_id_ptr, rdram, ctx);

    bool open = dino::debug_ui::begin_child(str_id);

    free(str_id);

    _return<s32>(ctx, open);
}

extern "C" void dbgui_end_child(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::end_child();
}

extern "C" void dbgui_checkbox(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(s32) value_ptr = _arg<1, PTR(s32)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    bool value = MEM_W(0, (gpr)value_ptr) != 0;

    bool pressed = dino::debug_ui::checkbox(label, &value);

    MEM_W(0, (gpr)value_ptr) = value;

    free(label);

    _return<s32>(ctx, pressed);
}

extern "C" void dbgui_input_int(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(s32) value_ptr = _arg<1, PTR(s32)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    s32 value = MEM_W(0, (gpr)value_ptr);

    bool pressed = dino::debug_ui::input_int(label, &value);

    MEM_W(0, (gpr)value_ptr) = value;

    free(label);

    _return<s32>(ctx, pressed);
}

extern "C" void dbgui_input_int_ext(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(s32) value_ptr = _arg<1, PTR(s32)>(rdram, ctx);
    PTR(void) options_ptr = _arg<2, PTR(void)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    s32 value = MEM_W(0, (gpr)value_ptr);

    int step = MEM_W(0x0, options_ptr);
    int step_fast = MEM_W(0x4, options_ptr);
    int flags = MEM_W(0x8, options_ptr);

    bool pressed = dino::debug_ui::input_int_ext(label, &value, step, step_fast, flags);

    MEM_W(0, (gpr)value_ptr) = value;

    free(label);

    _return<s32>(ctx, pressed);
}

extern "C" void dbgui_input_float(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    float *value_ptr = _arg<1, float*>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    bool pressed = dino::debug_ui::input_float(label, value_ptr);

    free(label);

    _return<s32>(ctx, pressed);
}

extern "C" void dbgui_input_float_ext(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    float *value_ptr = _arg<1, float*>(rdram, ctx);
    PTR(void) options_ptr = _arg<2, PTR(void)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    float step = MEM_F32(0x0, options_ptr);
    float step_fast = MEM_F32(0x4, options_ptr);
    PTR(char) format_ptr = MEM_W(0x8, options_ptr);
    int flags = MEM_W(0xC, options_ptr);

    char *format = format_ptr == NULL ? nullptr : dino::recomp_api::copy_rdram_str(format_ptr, rdram, ctx);

    bool pressed = dino::debug_ui::input_float_ext(label, value_ptr, step, step_fast, format, flags);

    free(label);
    if (format != nullptr) {
        free(format);
    }

    _return<s32>(ctx, pressed);
}

static std::vector<char> text_input_buffer{};

extern "C" void dbgui_input_text(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(char) buf_ptr = _arg<1, PTR(char)>(rdram, ctx);
    u32 buf_size = _arg<2, u32>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    text_input_buffer.resize(buf_size);
    for (size_t i = 0; i < buf_size; i++) {
        text_input_buffer.data()[i] = MEM_B(i, (gpr)buf_ptr);
    }

    bool changed = dino::debug_ui::input_text(label, text_input_buffer.data(), buf_size);

    if (changed) {
        for (size_t i = 0; i < buf_size; i++) {
            MEM_B(i, (gpr)buf_ptr) = text_input_buffer.data()[i];
        }
    }

    free(label);

    _return<s32>(ctx, changed);
}

extern "C" void dbgui_input_text_ext(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(char) buf_ptr = _arg<1, PTR(char)>(rdram, ctx);
    u32 buf_size = _arg<2, u32>(rdram, ctx);
    u32 flags = _arg<3, u32>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    text_input_buffer.resize(buf_size);
    for (size_t i = 0; i < buf_size; i++) {
        text_input_buffer.data()[i] = MEM_B(i, (gpr)buf_ptr);
    }

    bool changed = dino::debug_ui::input_text_ext(label, text_input_buffer.data(), buf_size, flags);

    if (changed) {
        for (size_t i = 0; i < buf_size; i++) {
            MEM_B(i, (gpr)buf_ptr) = text_input_buffer.data()[i];
        }
    }

    free(label);

    _return<s32>(ctx, changed);
}

extern "C" void dbgui_set_next_item_width(uint8_t* rdram, recomp_context* ctx) {
    float width = _arg<0, float>(rdram, ctx);

    dino::debug_ui::set_next_item_width(width);
}

extern "C" void dbgui_push_item_width(uint8_t* rdram, recomp_context* ctx) {
    float width = _arg<0, float>(rdram, ctx);

    dino::debug_ui::push_item_width(width);
}

extern "C" void dbgui_pop_item_width(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::pop_item_width();
}

extern "C" void dbgui_begin_tab_bar(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) id_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *id = dino::recomp_api::copy_rdram_str(id_ptr, rdram, ctx);

    bool active = dino::debug_ui::begin_tab_bar(id);

    free(id);

    _return<s32>(ctx, active);
}

extern "C" void dbgui_end_tab_bar(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::end_tab_bar();
}

extern "C" void dbgui_begin_tab_item(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) label_ptr = _arg<0, PTR(char)>(rdram, ctx);
    PTR(s32) open_ptr = _arg<1, PTR(s32)>(rdram, ctx);

    char *label = dino::recomp_api::copy_rdram_str(label_ptr, rdram, ctx);

    bool active = false;
    if ((gpr)open_ptr != 0) {
        bool open = MEM_W(0, (gpr)open_ptr) != 0;
        active = dino::debug_ui::begin_tab_item(label, &open);

        MEM_W(0, (gpr)open_ptr) = open;
    } else {
        active = dino::debug_ui::begin_tab_item(label, nullptr);
    }

    free(label);

    _return<s32>(ctx, active);
}

extern "C" void dbgui_end_tab_item(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::end_tab_item();
}

extern "C" void dbgui_push_str_id(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) id_ptr = _arg<0, PTR(char)>(rdram, ctx);

    char *id = dino::recomp_api::copy_rdram_str(id_ptr, rdram, ctx);

    dino::debug_ui::push_str_id(id);

    free(id);
}

extern "C" void dbgui_pop_id(uint8_t* rdram, recomp_context* ctx) {
    dino::debug_ui::pop_id();
}

extern "C" void dbgui_is_item_hovered(uint8_t* rdram, recomp_context* ctx) {
    bool hovered = dino::debug_ui::is_item_hovered();

    _return<s32>(ctx, hovered);
}

extern "C" void dbgui_get_display_size(uint8_t* rdram, recomp_context* ctx) {
    PTR(float) width_ptr = _arg<0, PTR(float)>(rdram, ctx);
    PTR(float) height_ptr = _arg<1, PTR(float)>(rdram, ctx);

    ImVec2 size = dino::debug_ui::get_display_size();

    MEM_F32(0x0, width_ptr) = size.x;
    MEM_F32(0x0, height_ptr) = size.y;
}

extern "C" void dbgui_color_float4_to_u32(uint8_t* rdram, recomp_context* ctx) {
    PTR(float) in_ptr = _arg<0, PTR(float)>(rdram, ctx);
    
    ImVec4 in(
        MEM_F32(0x0, in_ptr),
        MEM_F32(0x4, in_ptr),
        MEM_F32(0x8, in_ptr),
        MEM_F32(0xC, in_ptr)
    );

    ImU32 color = dino::debug_ui::color_float4_to_u32(in);

    _return<u32>(ctx, color);
}

extern "C" void dbgui_foreground_text(uint8_t* rdram, recomp_context* ctx) {
    PTR(float) pos_ptr = _arg<0, PTR(float)>(rdram, ctx);
    u32 color = _arg<1, u32>(rdram, ctx);
    PTR(char) text_ptr = _arg<2, PTR(char)>(rdram, ctx);
    
    ImVec2 pos(
        MEM_F32(0x0, pos_ptr),
        MEM_F32(0x4, pos_ptr)
    );

    char *text = dino::recomp_api::copy_rdram_str(text_ptr, rdram, ctx);

    dino::debug_ui::foreground_text(pos, color, text);

    free(text);
}

extern "C" void dbgui_foreground_line(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) ptr = _arg<0, PTR(void)>(rdram, ctx);
    
    ImVec2 p1(
        MEM_F32(0x0, ptr),
        MEM_F32(0x4, ptr)
    );
    ImVec2 p2(
        MEM_F32(0x8, ptr),
        MEM_F32(0xC, ptr)
    );
    ImU32 color = MEM_W(0x10, ptr);
    float thickness = MEM_F32(0x14, ptr);

    dino::debug_ui::foreground_line(p1, p2, color, thickness);
}

extern "C" void dbgui_foreground_circle(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) ptr = _arg<0, PTR(void)>(rdram, ctx);
    
    ImVec2 center(
        MEM_F32(0x0, ptr),
        MEM_F32(0x4, ptr)
    );
    float radius = MEM_F32(0x8, ptr);
    ImU32 color = MEM_W(0xC, ptr);
    s32 num_segments = MEM_W(0x10, ptr);
    float thickness = MEM_F32(0x14, ptr);

    dino::debug_ui::foreground_circle(center, radius, color, num_segments, thickness);
}

extern "C" void dbgui_foreground_circle_filled(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) ptr = _arg<0, PTR(void)>(rdram, ctx);
    
    ImVec2 center(
        MEM_F32(0x0, ptr),
        MEM_F32(0x4, ptr)
    );
    float radius = MEM_F32(0x8, ptr);
    ImU32 color = MEM_W(0xC, ptr);
    s32 num_segments = MEM_W(0x10, ptr);

    dino::debug_ui::foreground_circle_filled(center, radius, color, num_segments);
}

extern "C" void dbgui_foreground_ellipse(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) ptr = _arg<0, PTR(void)>(rdram, ctx);
    
    ImVec2 center(
        MEM_F32(0x0, ptr),
        MEM_F32(0x4, ptr)
    );
    ImVec2 radius(
        MEM_F32(0x8, ptr),
        MEM_F32(0xC, ptr)
    );
    ImU32 color = MEM_W(0x10, ptr);
    float rotation = MEM_F32(0x14, ptr);
    s32 num_segments = MEM_W(0x18, ptr);
    float thickness = MEM_F32(0x1C, ptr);

    dino::debug_ui::foreground_ellipse(center, radius, color, rotation, num_segments, thickness);
}

extern "C" void dbgui_foreground_ellipse_filled(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) ptr = _arg<0, PTR(void)>(rdram, ctx);
    
    ImVec2 center(
        MEM_F32(0x0, ptr),
        MEM_F32(0x4, ptr)
    );
    ImVec2 radius(
        MEM_F32(0x8, ptr),
        MEM_F32(0xC, ptr)
    );
    ImU32 color = MEM_W(0x10, ptr);
    float rotation = MEM_F32(0x14, ptr);
    s32 num_segments = MEM_W(0x18, ptr);

    dino::debug_ui::foreground_ellipse_filled(center, radius, color, rotation, num_segments);
}

extern "C" void dbgui_foreground_rect(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) ptr = _arg<0, PTR(void)>(rdram, ctx);
    
    ImVec2 pmin(
        MEM_F32(0x0, ptr),
        MEM_F32(0x4, ptr)
    );
    ImVec2 pmax(
        MEM_F32(0x8, ptr),
        MEM_F32(0xC, ptr)
    );
    ImU32 color = MEM_W(0x10, ptr);
    float thickness = MEM_F32(0x14, ptr);

    dino::debug_ui::foreground_rect(pmin, pmax, color, thickness);
}

extern "C" void dbgui_foreground_rect_filled(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) ptr = _arg<0, PTR(void)>(rdram, ctx);
    
    ImVec2 pmin(
        MEM_F32(0x0, ptr),
        MEM_F32(0x4, ptr)
    );
    ImVec2 pmax(
        MEM_F32(0x8, ptr),
        MEM_F32(0xC, ptr)
    );
    ImU32 color = MEM_W(0x10, ptr);

    dino::debug_ui::foreground_rect_filled(pmin, pmax, color);
}

namespace dino::recomp_api {
    void register_debug_ui_exports() {
        REGISTER_EXPORT(dbgui_is_open);
        REGISTER_EXPORT(dbgui_is_enabled);
        REGISTER_EXPORT(dbgui_begin);
        REGISTER_EXPORT(dbgui_end);
        REGISTER_EXPORT(dbgui_text);
        REGISTER_EXPORT(dbgui_label_text);
        REGISTER_EXPORT(dbgui_same_line);
        REGISTER_EXPORT(dbgui_new_line);
        REGISTER_EXPORT(dbgui_separator);
        REGISTER_EXPORT(dbgui_begin_combo);
        REGISTER_EXPORT(dbgui_end_combo);
        REGISTER_EXPORT(dbgui_selectable);
        REGISTER_EXPORT(dbgui_button);
        REGISTER_EXPORT(dbgui_begin_main_menu_bar);
        REGISTER_EXPORT(dbgui_end_main_menu_bar);
        REGISTER_EXPORT(dbgui_begin_menu);
        REGISTER_EXPORT(dbgui_end_menu);
        REGISTER_EXPORT(dbgui_menu_item);
        REGISTER_EXPORT(dbgui_collapsing_header);
        REGISTER_EXPORT(dbgui_tree_node);
        REGISTER_EXPORT(dbgui_tree_pop);
        REGISTER_EXPORT(dbgui_begin_child);
        REGISTER_EXPORT(dbgui_end_child);
        REGISTER_EXPORT(dbgui_checkbox);
        REGISTER_EXPORT(dbgui_input_int);
        REGISTER_EXPORT(dbgui_input_int_ext);
        REGISTER_EXPORT(dbgui_input_float);
        REGISTER_EXPORT(dbgui_input_float_ext);
        REGISTER_EXPORT(dbgui_input_text);
        REGISTER_EXPORT(dbgui_input_text_ext);
        REGISTER_EXPORT(dbgui_set_next_item_width);
        REGISTER_EXPORT(dbgui_push_item_width);
        REGISTER_EXPORT(dbgui_pop_item_width);
        REGISTER_EXPORT(dbgui_begin_tab_bar);
        REGISTER_EXPORT(dbgui_end_tab_bar);
        REGISTER_EXPORT(dbgui_begin_tab_item);
        REGISTER_EXPORT(dbgui_end_tab_item);
        REGISTER_EXPORT(dbgui_push_str_id);
        REGISTER_EXPORT(dbgui_pop_id);
        REGISTER_EXPORT(dbgui_is_item_hovered);
        REGISTER_EXPORT(dbgui_get_display_size);
        REGISTER_EXPORT(dbgui_color_float4_to_u32);
        REGISTER_EXPORT(dbgui_foreground_text);
        REGISTER_EXPORT(dbgui_foreground_line);
        REGISTER_EXPORT(dbgui_foreground_circle);
        REGISTER_EXPORT(dbgui_foreground_circle_filled);
        REGISTER_EXPORT(dbgui_foreground_ellipse);
        REGISTER_EXPORT(dbgui_foreground_ellipse_filled);
        REGISTER_EXPORT(dbgui_foreground_rect);
        REGISTER_EXPORT(dbgui_foreground_rect_filled);
    }
}
