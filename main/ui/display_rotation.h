#pragma once

#include "lvgl.h"

namespace ui {

/**
 * Rotates the picture by `quarter_turns` x 90 deg clockwise (square panels
 * only). Each rendered stripe is turned in software just before the
 * display's own flush callback gets it, together with its area and the
 * frame's dirty areas, so the driver keeps running in its native, unrotated
 * mode. Call once, right after the display is created, with the LVGL lock
 * held. 0 does nothing.
 */
void installDisplayRotation(lv_display_t* disp, int quarter_turns);

}  // namespace ui
