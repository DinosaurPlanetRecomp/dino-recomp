#pragma

#include "imgui.h"

namespace dino::debug_ui {

// Keep in sync with dbgui.h
typedef int DbgUiInputTextFlags;
enum DbgUiInputTextFlags_ {
    // Basic filters
    DBGUI_INPUT_TEXT_FLAGS_None                = 0,
    DBGUI_INPUT_TEXT_FLAGS_CharsDecimal        = 1 << 0,   // Allow 0123456789.+-*/
    DBGUI_INPUT_TEXT_FLAGS_CharsHexadecimal    = 1 << 1,   // Allow 0123456789ABCDEFabcdef
    DBGUI_INPUT_TEXT_FLAGS_CharsScientific     = 1 << 17,  // Allow 0123456789.+-*/eE (Scientific notation input)
    DBGUI_INPUT_TEXT_FLAGS_CharsUppercase      = 1 << 2,   // Turn a..z into A..Z
    DBGUI_INPUT_TEXT_FLAGS_CharsNoBlank        = 1 << 3,   // Filter out spaces, tabs

    // Inputs
    DBGUI_INPUT_TEXT_FLAGS_AllowTabInput       = 1 << 10,  // Pressing TAB input a '\t' character into the text field
    DBGUI_INPUT_TEXT_FLAGS_EnterReturnsTrue    = 1 << 5,   // Return 'true' when Enter is pressed (as opposed to every time the value was modified).
    DBGUI_INPUT_TEXT_FLAGS_EscapeClearsAll     = 1 << 20,  // Escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)

    // Other options
    DBGUI_INPUT_TEXT_FLAGS_ReadOnly            = 1 << 14,  // Read-only mode
    DBGUI_INPUT_TEXT_FLAGS_AlwaysOverwrite     = 1 << 13,  // Overwrite mode
    DBGUI_INPUT_TEXT_FLAGS_AutoSelectAll       = 1 << 4,   // Select entire text when first taking mouse focus
    DBGUI_INPUT_TEXT_FLAGS_ParseEmptyRefVal    = 1 << 15,  // dbgui_input_float(), dbgui_input_int() etc. only: parse empty string as zero value.

    // Elide display / Alignment
    DBGUI_INPUT_TEXT_FLAGS_ElideLeft           = 1 << 18,  // When text doesn't fit, elide left side to ensure right side stays visible.
};

bool is_open();

bool want_capture_keyboard();

void ui_frame_begin();
void ui_frame_end();

bool begin(const char *name, bool *open);
void end();

void text(const char *text);
void text_wrapped(const char *text);
void label_text(const char *label, const char *text);

void same_line();
void new_line();
void separator();
void separator_text(const char *label);
void indent(float width);
void unindent(float width);
void bullet();

bool begin_combo(const char *label, const char *preview);
void end_combo();
bool selectable(const char *label, bool *selected);
bool radio_button(const char *label, bool active);

bool button(const char *label);

bool begin_main_menu_bar();
void end_main_menu_bar();

bool begin_menu(const char *label);
void end_menu();

bool menu_item(const char *label, bool *selected);

bool collapsing_header(const char *label);
bool tree_node(const char *label);
void tree_pop();

bool begin_child(const char *str_id);
void end_child();

bool checkbox(const char *label, bool *v);
bool input_int(const char *label, int *v);
bool input_int_ext(const char *label, int *v, int step, int step_fast, DbgUiInputTextFlags flags);
bool input_float(const char *label, float *v);
bool input_float_ext(const char *label, float *v, float step, float step_fast, const char *format, DbgUiInputTextFlags flags);
bool input_text(const char *label, char *buf, size_t buf_size);
bool input_text_ext(const char *label, char *buf, size_t buf_size, DbgUiInputTextFlags flags);

void set_next_item_width(float width);
void push_item_width(float width);
void pop_item_width();

bool begin_tab_bar(const char *id);
void end_tab_bar();
bool begin_tab_item(const char *label, bool *open);
void end_tab_item();

void push_str_id(const char *id);
void pop_id();

bool is_item_hovered();

void set_item_tooltip(const char *text);

void begin_disabled(bool disabled);
void end_disabled();

ImVec2 get_display_size();

ImU32 color_float4_to_u32(const ImVec4 &in);

void foreground_text(const ImVec2 &pos, ImU32 color, const char *text);
void foreground_line(const ImVec2 &p1, const ImVec2 &p2, ImU32 color, float thickness);
void foreground_circle(const ImVec2 &center, float radius, ImU32 color, int num_segments, float thickness);
void foreground_circle_filled(const ImVec2 &center, float radius, ImU32 color, int num_segments);
void foreground_ellipse(const ImVec2 &center, const ImVec2 &radius, ImU32 color, float rot, int num_segments, float thickness);
void foreground_ellipse_filled(const ImVec2 &center, const ImVec2 &radius, ImU32 color, float rot, int num_segments);
void foreground_rect(const ImVec2 &p_min, const ImVec2 &p_max, ImU32 color, float thickness);
void foreground_rect_filled(const ImVec2 &p_min, const ImVec2 &p_max, ImU32 color);

}
