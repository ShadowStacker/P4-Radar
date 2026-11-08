#pragma once

#include <cstddef>

#include "core/aircraft_types.h"
#include "lvgl.h"

namespace ui::sprites {

/** The aircraft icon (see core/aircraft_types.h; icons by ADS-B Radar - https://adsb-radar.com). */
using Kind = core::aircraft::Icon;

constexpr int kRotationSteps = 72;  // 5 degree increments

/**
 * Sets the aircraft sprites' size, `scale_pct` % of the standard size, and
 * unpacks the icons. Call after LVGL init; calling again with another scale
 * drops the sprites made so far (only while nothing is drawing them).
 */
void init(int scale_pct = 100);

/**
 * Alpha-only (A8) sprite for the heading (degrees, 0 = north), drawn in the
 * image's recolor color. Made on first use, then kept. `small` is a ~60 %
 * set for crowded, zoomed-out views (much cheaper to draw).
 */
const lv_draw_buf_t* get(Kind kind, float heading_deg, bool small = false);
/** Sprite edge length in px (sprites are square, centered on the aircraft). */
int32_t size(Kind kind, bool small = false);

/**
 * Once a frame, outside drawing: keeps the sprite cache under its budget by
 * dropping the icons drawn longest ago (they are made again when needed).
 */
void trim(uint32_t now_ms);
/** Bytes of sprites cached now. */
size_t cacheBytes();

/**
 * White five-pointed star, `size` px square, for favorites (the built-in
 * fonts have no star glyph). Rendered once per size and cached.
 */
const lv_draw_buf_t* star(int32_t size);
int32_t starBadgeSize();   // radar badge next to a favorite aircraft
int32_t starButtonSize();  // star buttons in the info card and lists

/** White clock-face icon, `size` px square. Cached. */
const lv_draw_buf_t* clockIcon(int32_t size);
/** White camera icon (screenshots), `size` px square. Cached. */
const lv_draw_buf_t* cameraIcon(int32_t size);
/** White airplane icon (the airliner, nose up), `size` px square. Cached; unlike get(), never re-rendered. */
const lv_draw_buf_t* planeIcon(int32_t size);
/** White bookmark ribbon (a tall tab with a notch at the bottom), `size` px square. Cached. */
const lv_draw_buf_t* bookmarkIcon(int32_t size);
/** White magnifying glass (search), `size` px square. Cached. */
const lv_draw_buf_t* searchIcon(int32_t size);

/** The icon for an aircraft: by its ICAO type ("B738"), else its emitter category ("A3"). */
Kind kindFor(const char* type, const char* category);

}  // namespace ui::sprites
