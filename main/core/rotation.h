#pragma once

#include <cstdint>

namespace core {

/**
 * Maps a touch point from panel coordinates to the rotated UI's coordinates.
 * `quarter_turns` is the clockwise screen rotation (0-3), matching how the
 * display adapter maps UI pixels onto the panel; w/h are the panel size.
 */
inline void panelToUi(int px, int py, int quarter_turns, int w, int h, int* ux, int* uy) {
  switch (quarter_turns & 3) {
    case 1: *ux = py; *uy = w - 1 - px; break;
    case 2: *ux = w - 1 - px; *uy = h - 1 - py; break;
    case 3: *ux = h - 1 - py; *uy = px; break;
    default: *ux = px; *uy = py; break;
  }
}

/** Inverse of panelToUi(). */
inline void uiToPanel(int ux, int uy, int quarter_turns, int w, int h, int* px, int* py) {
  switch (quarter_turns & 3) {
    case 1: *px = w - 1 - uy; *py = ux; break;
    case 2: *px = w - 1 - ux; *py = h - 1 - uy; break;
    case 3: *px = uy; *py = h - 1 - ux; break;
    default: *px = ux; *py = uy; break;
  }
}

}  // namespace core
