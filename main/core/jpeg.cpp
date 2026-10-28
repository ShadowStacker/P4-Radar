#include "core/jpeg.h"

#include <cstring>
#include <vector>

#include "third_party/tjpgd/tjpgd.h"

namespace core::jpeg {

namespace {

struct Ctx {
  const uint8_t* data;
  size_t len;
  size_t pos;
  png::Image* out;
};

size_t input(JDEC* jd, uint8_t* buf, size_t n) {
  auto* c = static_cast<Ctx*>(jd->device);
  const size_t left = c->len - c->pos;
  if (n > left) n = left;
  if (buf) std::memcpy(buf, c->data + c->pos, n);
  c->pos += n;
  return n;
}

int output(JDEC* jd, void* bitmap, JRECT* r) {
  auto* c = static_cast<Ctx*>(jd->device);
  // JD_FORMAT 0 output. LVGL's copy of TJpgDec writes it in LVGL's memory
  // order, B-G-R (the original ChaN code writes R-G-B).
  const auto* src = static_cast<const uint8_t*>(bitmap);
  const int w = c->out->width;
  for (unsigned y = r->top; y <= r->bottom; ++y) {
    uint8_t* dst = &c->out->rgba[(static_cast<size_t>(y) * w + r->left) * 4];
    for (unsigned x = r->left; x <= r->right; ++x, src += 3, dst += 4) {
      dst[0] = src[2];
      dst[1] = src[1];
      dst[2] = src[0];
      dst[3] = 255;
    }
  }
  return 1;
}

}  // namespace

bool isJpeg(const uint8_t* data, size_t len) { return len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF; }

bool decode(const uint8_t* data, size_t len, png::Image* out, int max_pixels) {
  if (!isJpeg(data, len)) return false;
  std::vector<uint8_t> pool(4096);  // TJpgDec work area (~3.5 KB at JD_FASTDECODE 1)
  Ctx ctx{data, len, 0, out};
  JDEC jd;
  if (jd_prepare(&jd, input, pool.data(), pool.size(), &ctx) != JDR_OK) return false;
  if (jd.width == 0 || jd.height == 0 || static_cast<long long>(jd.width) * jd.height > max_pixels) return false;
  out->width = jd.width;
  out->height = jd.height;
  out->rgba.assign(static_cast<size_t>(jd.width) * jd.height * 4, 0);
  return jd_decomp(&jd, output, 0) == JDR_OK;
}

}  // namespace core::jpeg
