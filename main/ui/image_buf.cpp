#include "ui/image_buf.h"

#include <cstring>

#include "platform/platform.h"

namespace ui {

lv_draw_buf_t* createImageBuf(uint32_t w, uint32_t h, lv_color_format_t cf) {
  const uint32_t stride = lv_draw_buf_width_to_stride(w, cf);
  const uint32_t data_size = stride * h;
  const size_t header = (sizeof(lv_draw_buf_t) + 63) / 64 * 64;  // keep pixels 64-byte aligned
  auto* block = static_cast<uint8_t*>(plat::allocLarge(header + data_size));
  if (block == nullptr) return nullptr;
  auto* buf = reinterpret_cast<lv_draw_buf_t*>(block);
  std::memset(buf, 0, sizeof(*buf));
  if (lv_draw_buf_init(buf, w, h, cf, stride, block + header, data_size) != LV_RESULT_OK) {
    plat::freeLarge(block);
    return nullptr;
  }
  return buf;
}

void destroyImageBuf(lv_draw_buf_t* buf) {
  if (buf != nullptr) plat::freeLarge(buf);  // header is the start of the block
}

}  // namespace ui
