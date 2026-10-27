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

struct Rect {
  int x1, y1, x2, y2;  // inclusive
};

/** A UI rectangle's footprint on the panel. */
inline Rect uiRectToPanel(const Rect& r, int quarter_turns, int w, int h) {
  int ax, ay, bx, by;
  uiToPanel(r.x1, r.y1, quarter_turns, w, h, &ax, &ay);
  uiToPanel(r.x2, r.y2, quarter_turns, w, h, &bx, &by);
  return {ax < bx ? ax : bx, ay < by ? ay : by, ax < bx ? bx : ax, ay < by ? by : ay};
}

/**
 * Rotates a block of pixels the way uiToPanel() maps coordinates. `src` is
 * sw x sh with a row stride of `src_stride` pixels; `dst` is packed and is
 * sh x sw for odd quarter turns, sw x sh otherwise. Works in tiles so both
 * sides stay cache-friendly.
 */
template <typename T>
void rotatePixels(const T* src, int sw, int sh, int src_stride, T* dst, int quarter_turns) {
  const int q = quarter_turns & 3;
  const int dw = (q & 1) ? sh : sw;
  constexpr int kTile = 32;
  for (int ty = 0; ty < sh; ty += kTile) {
    const int ye = ty + kTile < sh ? ty + kTile : sh;
    for (int tx = 0; tx < sw; tx += kTile) {
      const int xe = tx + kTile < sw ? tx + kTile : sw;
      for (int y = ty; y < ye; ++y) {
        const T* row = src + y * src_stride;
        for (int x = tx; x < xe; ++x) {
          int dx, dy;
          switch (q) {
            case 1: dx = sh - 1 - y; dy = x; break;
            case 2: dx = sw - 1 - x; dy = sh - 1 - y; break;
            case 3: dx = y; dy = sw - 1 - x; break;
            default: dx = x; dy = y; break;
          }
          dst[dy * dw + dx] = row[x];
        }
      }
    }
  }
}

}  // namespace core
