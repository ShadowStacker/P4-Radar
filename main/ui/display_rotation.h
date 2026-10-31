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

/** Time spent handing rendered stripes to the panel (rotation included), since start. */
struct FlushStats {
  uint64_t rotate_us = 0;  // turning the pixels
  uint64_t flush_us = 0;   // all of it: rotation plus the driver's copy to the frame buffer
  uint32_t stripes = 0;
};
FlushStats flushStats();

}  // namespace ui
