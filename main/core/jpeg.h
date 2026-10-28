#pragma once

#include <cstddef>
#include <cstdint>

#include "core/png.h"  // core::png::Image

namespace core::jpeg {

/** True if the data starts like a JPEG (FF D8). */
bool isJpeg(const uint8_t* data, size_t len);

/**
 * Decodes a baseline JPEG to RGBA (alpha 255), using ChaN's TJpgDec
 * (third_party/tjpgd). Progressive JPEGs are rejected.
 */
bool decode(const uint8_t* data, size_t len, png::Image* out, int max_pixels = 1024 * 1024);

}  // namespace core::jpeg
