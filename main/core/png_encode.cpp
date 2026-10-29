#include <algorithm>
#include <cstdlib>
#include <vector>

#include "core/deflate.h"
#include "core/png.h"

namespace core::png {

namespace {

void chunk(std::string* out, const char* type, const std::string& data) {
  const uint32_t n = static_cast<uint32_t>(data.size());
  for (int s = 24; s >= 0; s -= 8) out->push_back(static_cast<char>(n >> s));
  std::string td(type, 4);
  td += data;
  *out += td;
  const uint32_t crc = crc32(0, td.data(), td.size());
  for (int s = 24; s >= 0; s -= 8) out->push_back(static_cast<char>(crc >> s));
}

}  // namespace

std::string encodeRgb(const uint8_t* px, int width, int height, int stride, bool bgr) {
  const size_t row = static_cast<size_t>(width) * 3;
  std::vector<uint8_t> raw((row + 1) * static_cast<size_t>(height));
  std::vector<uint8_t> cur(row), prev(row, 0), cand(row);
  for (int y = 0; y < height; ++y) {
    const uint8_t* src = px + static_cast<size_t>(y) * static_cast<size_t>(stride);
    for (int x = 0; x < width; ++x) {
      cur[x * 3 + 0] = src[x * 3 + (bgr ? 2 : 0)];
      cur[x * 3 + 1] = src[x * 3 + 1];
      cur[x * 3 + 2] = src[x * 3 + (bgr ? 0 : 2)];
    }
    // Pick the filter (none, sub, up) with the smallest sum of magnitudes.
    uint8_t* dst = &raw[static_cast<size_t>(y) * (row + 1)];
    long best_sum = -1;
    for (int f = 0; f <= 2; ++f) {
      long sum = 0;
      for (size_t i = 0; i < row; ++i) {
        const uint8_t left = i >= 3 ? cur[i - 3] : 0;
        const uint8_t v = f == 0 ? cur[i] : static_cast<uint8_t>(cur[i] - (f == 1 ? left : prev[i]));
        cand[i] = v;
        sum += v < 128 ? v : 256 - v;
      }
      if (best_sum < 0 || sum < best_sum) {
        best_sum = sum;
        dst[0] = static_cast<uint8_t>(f);
        std::copy(cand.begin(), cand.end(), dst + 1);
      }
    }
    prev.swap(cur);
  }
  std::string out("\x89PNG\r\n\x1a\n", 8);
  std::string ihdr;
  for (int v : {width, height}) {
    for (int s = 24; s >= 0; s -= 8) ihdr.push_back(static_cast<char>(v >> s));
  }
  ihdr += std::string("\x08\x02\x00\x00\x00", 5);  // 8-bit RGB
  chunk(&out, "IHDR", ihdr);
  chunk(&out, "IDAT", zlibCompress(raw.data(), raw.size()));
  chunk(&out, "IEND", "");
  return out;
}

}  // namespace core::png
