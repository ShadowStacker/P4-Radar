#pragma once

#include <cstdint>

#include "core/settings.h"
#include "lvgl.h"

namespace ui {

struct Palette {
  lv_color_t bg;          // radar background
  lv_color_t bg_center;   // slightly brighter middle (vignette)
  lv_color_t grid;        // range rings / crosshair
  lv_color_t grid_text;   // ring labels
  lv_color_t bezel;       // compass ticks
  lv_color_t bezel_text;
  lv_color_t sweep;       // radar beam
  lv_color_t aircraft;    // symbols in "classic" color mode
  lv_color_t trail;
  lv_color_t vector;      // speed vectors
  lv_color_t label;       // aircraft callsign
  lv_color_t label_dim;   // aircraft second line
  lv_color_t runway;
  lv_color_t runway_label;
  lv_color_t home;
  lv_color_t select;
  lv_color_t favorite;    // saved aircraft (ring, star, label)
  lv_color_t panel;       // HUD panels / settings rows
  lv_color_t panel_border;
  lv_color_t text;
  lv_color_t text_dim;
  lv_color_t accent;
  lv_color_t warn;
  lv_color_t danger;
};

const Palette& pal();
/** Builds the palette for st.theme (Custom uses the st.custom_* colors) and the label size. */
void setTheme(const core::Settings& st);
/** The palette a Custom theme with these settings would have (for previews). */
Palette customPalette(const core::Settings& st);

/** Scales a size designed for the 800 px panel to the actual resolution. */
int32_t sc(int32_t px_at_800);
void setScreenSize(int32_t w, int32_t h);
int32_t screenW();
int32_t screenH();

/** tar1090-style altitude color (orange low -> green -> blue -> magenta high). */
lv_color_t altitudeColor(int32_t alt_ft, bool on_ground);

enum class FontSize { S, M, L, XL, XXL };
const lv_font_t* font(FontSize s);
/** Font for aircraft labels on the radar (Display -> Label size). */
const lv_font_t* labelFont();

/** Applies the theme to an LVGL theme + default styles; call after setTheme. */
void applyLvglTheme(lv_display_t* disp);

}  // namespace ui
