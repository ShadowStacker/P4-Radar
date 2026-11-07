#pragma once

#include <cstdint>

namespace core::sprite {

/**
 * Draws a square alpha mask (`src`, src_size x src_size, nose up) into
 * `dst` (dst_size x dst_size, `dst_stride` bytes a row), rotated clockwise
 * by `heading_deg` about the center and scaled to fit. Each pixel averages
 * 3 x 3 bilinear samples, so shrinking stays smooth. Pure CPU: safe at any
 * time, even in the middle of LVGL drawing.
 */
void rotateMask(const uint8_t* src, int src_size, uint8_t* dst, int dst_size, int dst_stride, float heading_deg);

}  // namespace core::sprite
