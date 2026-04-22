#pragma once

#include "patch_helpers.h"

#include "PR/ultratypes.h"

typedef struct {
    f32 x, y;
} DbgUiPos;

// Keep in sync with ImGuiInputTextFlags
// Note: Not all options are currently supported in DbgUi
typedef enum {
    DBGUI_INPUT_TEXT_FLAGS_None                = 0,
    DBGUI_INPUT_TEXT_FLAGS_CharsDecimal        = 1 << 0,   // Allow 0123456789.+-*/
    DBGUI_INPUT_TEXT_FLAGS_CharsHexadecimal    = 1 << 1,   // Allow 0123456789ABCDEFabcdef
    DBGUI_INPUT_TEXT_FLAGS_CharsUppercase      = 1 << 2,   // Turn a..z into A..Z
    DBGUI_INPUT_TEXT_FLAGS_CharsNoBlank        = 1 << 3,   // Filter out spaces, tabs
    DBGUI_INPUT_TEXT_FLAGS_AutoSelectAll       = 1 << 4,   // Select entire text when first taking mouse focus
    DBGUI_INPUT_TEXT_FLAGS_EnterReturnsTrue    = 1 << 5,   // Return 'true' when Enter is pressed (as opposed to every time the value was modified).
    //DBGUI_INPUT_TEXT_FLAGS_CallbackCompletion  = 1 << 6,   // Callback on pressing TAB (for completion handling)
    //DBGUI_INPUT_TEXT_FLAGS_CallbackHistory     = 1 << 7,   // Callback on pressing Up/Down arrows (for history handling)
    //DBGUI_INPUT_TEXT_FLAGS_CallbackAlways      = 1 << 8,   // Callback on each iteration. User code may query cursor position, modify text buffer.
    //DBGUI_INPUT_TEXT_FLAGS_CallbackCharFilter  = 1 << 9,   // Callback on character inputs to replace or discard them. Modify 'EventChar' to replace or discard, or return 1 in callback to discard.
    DBGUI_INPUT_TEXT_FLAGS_AllowTabInput       = 1 << 10,  // Pressing TAB input a '\t' character into the text field
    DBGUI_INPUT_TEXT_FLAGS_CtrlEnterForNewLine = 1 << 11,  // In multi-line mode, unfocus with Enter, add new line with Ctrl+Enter (default is opposite: unfocus with Ctrl+Enter, add line with Enter).
    DBGUI_INPUT_TEXT_FLAGS_NoHorizontalScroll  = 1 << 12,  // Disable following the cursor horizontally
    DBGUI_INPUT_TEXT_FLAGS_AlwaysOverwrite     = 1 << 13,  // Overwrite mode
    DBGUI_INPUT_TEXT_FLAGS_ReadOnly            = 1 << 14,  // Read-only mode
    DBGUI_INPUT_TEXT_FLAGS_Password            = 1 << 15,  // Password mode, display all characters as '*'
    DBGUI_INPUT_TEXT_FLAGS_NoUndoRedo          = 1 << 16,  // Disable undo/redo. Note that input text owns the text data while active, if you want to provide your own undo/redo stack you need e.g. to call ClearActiveID().
    DBGUI_INPUT_TEXT_FLAGS_CharsScientific     = 1 << 17,  // Allow 0123456789.+-*/eE (Scientific notation input)
    //DBGUI_INPUT_TEXT_FLAGS_CallbackResize      = 1 << 18,  // Callback on buffer capacity changes request (beyond 'buf_size' parameter value), allowing the string to grow. Notify when the string wants to be resized (for string types which hold a cache of their Size). You will be provided a new BufSize in the callback and NEED to honor it. (see misc/cpp/imgui_stdlib.h for an example of using this)
    //DBGUI_INPUT_TEXT_FLAGS_CallbackEdit        = 1 << 19,  // Callback on any edit (note that InputText() already returns true on edit, the callback is useful mainly to manipulate the underlying buffer while focus is active)
    DBGUI_INPUT_TEXT_FLAGS_EscapeClearsAll     = 1 << 20,  // Escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
} DbgUiInputTextFlags;

DECLARE_FUNC(void, dbgui_ui_frame_begin, void);
DECLARE_FUNC(void, dbgui_ui_frame_end, void);
DECLARE_FUNC(s32, dbgui_begin, const char *str, s32 *open);
DECLARE_FUNC(void, dbgui_end, void);
DECLARE_FUNC(void, dbgui_text, const char *text);
DECLARE_FUNC(void, dbgui_text_wrapped, const char *text);
DECLARE_FUNC(void, dbgui_label_text, const char *label, const char *text);
DECLARE_FUNC(void, dbgui_same_line, void);
DECLARE_FUNC(void, dbgui_new_line, void);
DECLARE_FUNC(void, dbgui_separator, void);
DECLARE_FUNC(void, dbgui_separator_text, const char *label);
DECLARE_FUNC(void, dbgui_indent, f32 width);
DECLARE_FUNC(void, dbgui_unindent, f32 width);
DECLARE_FUNC(void, dbgui_bullet, void);
DECLARE_FUNC(s32, dbgui_begin_combo, const char *label, const char *preview);
DECLARE_FUNC(void, dbgui_end_combo, void);
DECLARE_FUNC(s32, dbgui_selectable, const char *label, s32 selected);
DECLARE_FUNC(s32, dbgui_radio_button, const char *label, s32 active);
DECLARE_FUNC(s32, dbgui_button, const char *label);
DECLARE_FUNC(s32, dbgui_is_open, void);
DECLARE_FUNC(s32, dbgui_begin_main_menu_bar, void);
DECLARE_FUNC(void, dbgui_end_main_menu_bar, void);
DECLARE_FUNC(s32, dbgui_begin_menu, const char *label);
DECLARE_FUNC(void, dbgui_end_menu, void);
DECLARE_FUNC(s32, dbgui_menu_item, const char *label, s32 *selected);
DECLARE_FUNC(s32, dbgui_collapsing_header, const char *label);
DECLARE_FUNC(s32, dbgui_tree_node, const char *label);
DECLARE_FUNC(void, dbgui_tree_pop, void);
DECLARE_FUNC(s32, dbgui_begin_child, const char *str_id);
DECLARE_FUNC(void, dbgui_end_child, void);
DECLARE_FUNC(s32, dbgui_checkbox, const char *label, s32 *value);
typedef struct {
    s32 step;
    s32 stepFast;
    DbgUiInputTextFlags flags;
} DbgUiInputIntOptions;
DECLARE_FUNC(s32, dbgui_input_int, const char *label, s32 *value);
DECLARE_FUNC(s32, dbgui_input_int_ext, const char *label, s32 *value, const DbgUiInputIntOptions *options);
typedef struct {
    f32 step;
    f32 stepFast;
    const char *format;
    DbgUiInputTextFlags flags;
} DbgUiInputFloatOptions;
DECLARE_FUNC(s32, dbgui_input_float, const char *label, f32 *value);
DECLARE_FUNC(s32, dbgui_input_float_ext, const char *label, f32 *value, const DbgUiInputFloatOptions *options);
DECLARE_FUNC(s32, dbgui_input_text, const char *label, char *buffer, u32 buffer_size);
DECLARE_FUNC(s32, dbgui_input_text_text, const char *label, char *buffer, u32 buffer_size, DbgUiInputTextFlags flags);
DECLARE_FUNC(void, dbgui_set_next_item_width, f32 width);
DECLARE_FUNC(void, dbgui_push_item_width, f32 width);
DECLARE_FUNC(void, dbgui_pop_item_width, void);
DECLARE_FUNC(s32, dbgui_begin_tab_bar, const char *str_id);
DECLARE_FUNC(void, dbgui_end_tab_bar, void);
DECLARE_FUNC(s32, dbgui_begin_tab_item, const char *label, s32 *open);
DECLARE_FUNC(void, dbgui_end_tab_item, void);
DECLARE_FUNC(void, dbgui_push_str_id, const char *str_id);
DECLARE_FUNC(void, dbgui_pop_id, void);
DECLARE_FUNC(s32, dbgui_is_item_hovered, void);
DECLARE_FUNC(void, dbgui_set_item_tooltip, const char *text);
DECLARE_FUNC(void, dbgui_begin_disabled, s32 disabled);
DECLARE_FUNC(void, dbgui_end_disabled, void);
DECLARE_FUNC(void, dbgui_get_display_size, f32 *width, f32 *height);
typedef struct {
    float r;
    float g;
    float b;
    float a;
} DbgUiColor;
DECLARE_FUNC(u32, dbgui_color_float4_to_u32, const DbgUiColor *color);
DECLARE_FUNC(void, dbgui_foreground_text, const DbgUiPos *pos, u32 color, const char *text);
typedef struct {
    DbgUiPos p1;
    DbgUiPos p2;
    u32 color;
    float thickness;
} DbgUiLine;
DECLARE_FUNC(void, dbgui_foreground_line, const DbgUiLine *line);
typedef struct {
    DbgUiPos center;
    float radius;
    u32 color;
    s32 numSegments;
    float thickness;
} DbgUiCircle;
DECLARE_FUNC(void, dbgui_foreground_circle, const DbgUiCircle *circle);
typedef struct {
    DbgUiPos center;
    float radius;
    u32 color;
    s32 numSegments;
} DbgUiCircleFilled;
DECLARE_FUNC(void, dbgui_foreground_circle_filled, const DbgUiCircleFilled *circle);
typedef struct {
    DbgUiPos center;
    float radiusX;
    float radiusY;
    u32 color;
    float rotation;
    s32 numSegments;
    float thickness;
} DbgUiEllipse;
DECLARE_FUNC(void, dbgui_foreground_ellipse, const DbgUiEllipse *ellipse);
typedef struct {
    DbgUiPos center;
    float radiusX;
    float radiusY;
    u32 color;
    float rotation;
    s32 numSegments;
} DbgUiEllipseFilled;
DECLARE_FUNC(void, dbgui_foreground_ellipse_filled, const DbgUiEllipseFilled *ellipse);
typedef struct {
    DbgUiPos min;
    DbgUiPos max;
    u32 color;
    float thickness;
} DbgUiRect;
DECLARE_FUNC(void, dbgui_foreground_rect, const DbgUiRect *rect);
typedef struct {
    DbgUiPos min;
    DbgUiPos max;
    u32 color;
} DbgUiRectFilled;
DECLARE_FUNC(void, dbgui_foreground_rect_filled, const DbgUiRectFilled *rect);

void dbgui_textf(const char *fmt, ...);
void dbgui_label_textf(const char *label, const char *fmt, ...);
