#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace core::png {

/** Decoded image: RGBA, 8 bits per channel, rows packed. */
struct Image {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;
};

/**
 * Decodes a PNG: 8-bit gray, RGB, gray+alpha and RGBA, and palette images at
 * 1/2/4/8 bits (what map tile servers send). Interlaced and 16-bit images
 * are rejected. `max_pixels` guards against huge images.
 */
bool decode(const uint8_t* data, size_t len, Image* out, int max_pixels = 1024 * 1024);

}  // namespace core::png

namespace core::png {

/**
 * Encodes 8-bit RGB pixels as a PNG (rows `stride` bytes apart). With `bgr`
 * the input is B, G, R per pixel, as LVGL's RGB888 buffers are.
 */
std::string encodeRgb(const uint8_t* px, int width, int height, int stride, bool bgr);

}  // namespace core::png
