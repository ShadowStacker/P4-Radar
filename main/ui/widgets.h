#pragma once

#include <functional>
#include <string>
#include <vector>

#include "lvgl.h"

namespace ui {

using Callback = std::function<void()>;

/** Attaches a std::function click handler (freed with the object). */
void onClick(lv_obj_t* obj, Callback cb);
/** Attaches a handler for an arbitrary event code. */
void onEvent(lv_obj_t* obj, lv_event_code_t code, Callback cb);

/** Round translucent icon button (LV_SYMBOL_* or short text). */
lv_obj_t* iconButton(lv_obj_t* parent, const char* symbol, int32_t diameter, Callback cb);
/** Pill-shaped text button. */
lv_obj_t* pillButton(lv_obj_t* parent, const char* text, Callback cb, bool accent = false);
/** Translucent rounded panel. */
lv_obj_t* panel(lv_obj_t* parent);
lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* f, lv_color_t c);

/**
 * Modal confirm dialog centered on screen. `on_yes` runs when confirmed.
 */
void confirm(const char* title, const char* body, const char* yes_text, Callback on_yes);
/** Short toast message at the top of the screen. */
void toast(const char* text, uint32_t ms = 2000);

}  // namespace ui
