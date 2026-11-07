#include "core/sprite_raster.h"

#include <cmath>

namespace core::sprite {

void rotateMask(const uint8_t* src, int src_size, uint8_t* dst, int dst_size, int dst_stride, float heading_deg) {
  const float r = heading_deg * 3.14159265f / 180.0f;
  const float cs = std::cos(r), sn = std::sin(r);
  const float k = static_cast<float>(src_size) / static_cast<float>(dst_size);
  const float dc = dst_size / 2.0f, sc = src_size / 2.0f;
  auto at = [&](int x, int y) -> float {
    return x < 0 || y < 0 || x >= src_size || y >= src_size ? 0.0f : static_cast<float>(src[y * src_size + x]);
  };
  constexpr int kSub = 3;
  for (int y = 0; y < dst_size; ++y) {
    uint8_t* row = dst + y * dst_stride;
    for (int x = 0; x < dst_size; ++x) {
      float sum = 0;
      for (int sy = 0; sy < kSub; ++sy) {
        for (int sx = 0; sx < kSub; ++sx) {
          const float dx = x + (sx + 0.5f) / kSub - dc;
          const float dy = y + (sy + 0.5f) / kSub - dc;
          // Undo the clockwise turn (screen y points down), then scale up to the mask.
          const float u = (dx * cs + dy * sn) * k + sc - 0.5f;
          const float v = (-dx * sn + dy * cs) * k + sc - 0.5f;
          const int x0 = static_cast<int>(std::floor(u)), y0 = static_cast<int>(std::floor(v));
          const float fx = u - x0, fy = v - y0;
          sum += (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy) +
                 (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy;
        }
      }
      const int v = static_cast<int>(sum / (kSub * kSub) + 0.5f);
      row[x] = static_cast<uint8_t>(v > 255 ? 255 : v);
    }
  }
}

}  // namespace core::sprite
