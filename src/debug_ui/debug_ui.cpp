#include "debug_ui.hpp"
#include "backend.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace dino::debug_ui {

bool is_open() {
    return backend::b_is_open;
}

void set_is_open(bool open) {
    backend::b_is_open = open;
}

bool want_capture_keyboard() {
    // Go directly through our context pointer so we don't have to worry about it being current
    if (backend::dino_imgui_ctx == nullptr) {
        return false;
    }

    return backend::b_is_open && backend::dino_imgui_ctx->IO.WantCaptureKeyboard;
}

void ui_frame_begin() {
    backend::begin();
}

void ui_frame_end() {
    backend::end();
}

static void assert_is_open() {
    assert((!backend::b_is_open || backend::b_in_ui_frame) && "Cannot call debug UI functions while outside of the debug UI frame.");
    // b_is_open can be set false concurrently while in the middle of calling ImGui functions,
    // don't fail the assert and instead let the frame finish.
    assert((backend::b_is_open || backend::b_in_ui_frame) && "Cannot call debug UI functions while the debug UI is closed.");
}

bool begin(const char *name, bool *open) {
    assert_is_open();
    return ImGui::Begin(name, open);
}

void end() {
    assert_is_open();
    ImGui::End();
}

void text(const char *text) {
    assert_is_open();
    ImGui::Text("%s", text);
}

void label_text(const char *label, char *text) {
    assert_is_open();
    ImGui::LabelText(label, "%s", text);
}

void same_line() {
    assert_is_open();
    ImGui::SameLine();
}

bool begin_combo(const char *label, char *preview) {
    assert_is_open();
    return ImGui::BeginCombo(label, preview);
}

void end_combo() {
    assert_is_open();
    ImGui::EndCombo();
}

bool selectable(const char *label, bool *selected) {
    assert_is_open();
    return ImGui::Selectable(label, selected);
}

bool button(const char *label) {
    assert_is_open();
    return ImGui::Button(label);
}

bool begin_main_menu_bar() {
    assert_is_open();
    return ImGui::BeginMainMenuBar();
}

void end_main_menu_bar() {
    assert_is_open();
    ImGui::EndMainMenuBar();
}

bool begin_menu(const char *label) {
    assert_is_open();
    return ImGui::BeginMenu(label);
}

void end_menu() {
    assert_is_open();
    ImGui::EndMenu();
}

bool menu_item(const char *label, bool *selected) {
    assert_is_open();
    return ImGui::MenuItem(label, NULL, selected);
}

bool collapsing_header(const char *label) {
    assert_is_open();
    return ImGui::CollapsingHeader(label);
}

bool tree_node(const char *label) {
    assert_is_open();
    return ImGui::TreeNode(label);
}

void tree_pop() {
    assert_is_open();
    ImGui::TreePop();
}

bool begin_child(const char *str_id) {
    assert_is_open();
    return ImGui::BeginChild(str_id);
}

void end_child() {
    assert_is_open();
    ImGui::EndChild();
}

bool checkbox(const char *label, bool *v) {
    assert_is_open();
    return ImGui::Checkbox(label, v);
}

bool input_int(const char *label, int *v) {
    assert_is_open();
    return ImGui::InputInt(label, v, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue);
}

bool input_float(const char *label, float *v) {
    assert_is_open();
    return ImGui::InputFloat(label, v, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue);
}

bool input_text(const char *label, char *buf, int buf_size) {
    assert_is_open();
    return ImGui::InputText(label, buf, buf_size);
}

void set_next_item_width(float width) {
    assert_is_open();
    ImGui::SetNextItemWidth(width);
}

void push_item_width(float width) {
    assert_is_open();
    ImGui::PushItemWidth(width);
}

void pop_item_width() {
    assert_is_open();
    ImGui::PopItemWidth();
}

bool begin_tab_bar(const char *id) {
    assert_is_open();
    return ImGui::BeginTabBar(id);
}

void end_tab_bar() {
    assert_is_open();
    ImGui::EndTabBar();
}

bool begin_tab_item(const char *label, bool *open) {
    assert_is_open();
    return ImGui::BeginTabItem(label, open);
}

void end_tab_item() {
    assert_is_open();
    ImGui::EndTabItem();
}

void push_str_id(const char *id) {
    assert_is_open();
    ImGui::PushID(id);
}

void pop_id() {
    assert_is_open();
    ImGui::PopID();
}

bool is_item_hovered() {
    assert_is_open();
    return ImGui::IsItemHovered();
}

ImVec2 get_display_size() {
    assert_is_open();
    return ImGui::GetIO().DisplaySize;
}

ImU32 color_float4_to_u32(const ImVec4 &in) {
    return ImGui::ColorConvertFloat4ToU32(in);
}

void foreground_text(const ImVec2 &pos, ImU32 color, const char *text) {
    assert_is_open();
    ImGui::GetForegroundDrawList()->AddText(pos, color, text);
}

void foreground_line(const ImVec2 &p1, const ImVec2 &p2, ImU32 color, float thickness) {
    assert_is_open();
    ImGui::GetForegroundDrawList()->AddLine(p1, p2, color, thickness);
}

void foreground_circle(const ImVec2 &center, float radius, ImU32 color, int num_segments, float thickness) {
    assert_is_open();
    ImGui::GetForegroundDrawList()->AddCircle(center, radius, color, num_segments, thickness);
}

void foreground_circle_filled(const ImVec2 &center, float radius, ImU32 color, int num_segments) {
    assert_is_open();
    ImGui::GetForegroundDrawList()->AddCircleFilled(center, radius, color, num_segments);
}

void foreground_ellipse(const ImVec2 &center, float radius_x, float radius_y, ImU32 color, float rot, int num_segments, float thickness) {
    assert_is_open();
    ImGui::GetForegroundDrawList()->AddEllipse(center, radius_x, radius_y, color, rot, num_segments, thickness);
}

void foreground_ellipse_filled(const ImVec2 &center, float radius_x, float radius_y, ImU32 color, float rot, int num_segments) {
    assert_is_open();
    ImGui::GetForegroundDrawList()->AddEllipseFilled(center, radius_x, radius_y, color, rot, num_segments);
}

void foreground_rect(const ImVec2 &p_min, const ImVec2 &p_max, ImU32 color, float thickness) {
    assert_is_open();
    ImGui::GetForegroundDrawList()->AddRect(p_min, p_max, color, 0, 0, thickness);
}

void foreground_rect_filled(const ImVec2 &p_min, const ImVec2 &p_max, ImU32 color) {
    assert_is_open();
    ImGui::GetForegroundDrawList()->AddRectFilled(p_min, p_max, color);
}

}
