#pragma once

#include "lvgl.h"

namespace ui::sprites {

enum class Kind : uint8_t { Light = 0, Medium, Heavy, Rotor, Ground, kCount };

constexpr int kRotationSteps = 72;  // 5 degree increments

/** Renders all aircraft sprites (white, to be recolored). Call once after LVGL init. */
void init();

/** White ARGB8888 sprite for the heading (degrees, 0 = north). */
const lv_draw_buf_t* get(Kind kind, float heading_deg);
/** Sprite edge length in px (sprites are square, centered on the aircraft). */
int32_t size(Kind kind);

/**
 * White five-pointed star, `size` px square, for favorites (the built-in
 * fonts have no star glyph). Rendered once per size and cached.
 */
const lv_draw_buf_t* star(int32_t size);
int32_t starBadgeSize();   // radar badge next to a favorite aircraft
int32_t starButtonSize();  // star buttons in the info card and lists

/** Picks a sprite family from the ADS-B emitter category (A1..A7, C1..). */
Kind kindFor(const char* category, bool on_ground);

}  // namespace ui::sprites
