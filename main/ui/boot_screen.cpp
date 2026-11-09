#include "ui/boot_screen.h"

#include <cstring>

#include "core/png.h"
#include "data/biffo_portrait.h"
#include "ui/image_buf.h"
// lv_image_cache_drop(): its header moved between LVGL 9.2 and 9.6.
#if __has_include("src/misc/cache/instance/lv_image_cache.h")
#include "src/misc/cache/instance/lv_image_cache.h"
#elif __has_include("src/misc/cache/lv_image_cache.h")
#include "src/misc/cache/lv_image_cache.h"
#endif

namespace ui::boot {

namespace {

lv_obj_t* s_status = nullptr;
lv_draw_buf_t* s_portrait = nullptr;

/** Decodes the embedded PNG into an ARGB8888 draw buffer (PSRAM on the device). */
lv_draw_buf_t* decodePortrait() {
  core::png::Image img;
  if (!core::png::decode(data::kBiffoPortraitPng, data::kBiffoPortraitPngLen, &img, 512 * 512)) return nullptr;
  lv_draw_buf_t* buf = createImageBuf(img.width, img.height, LV_COLOR_FORMAT_ARGB8888);
  if (buf == nullptr) return nullptr;
  for (int y = 0; y < img.height; ++y) {
    const uint8_t* src = img.rgba.data() + static_cast<size_t>(y) * img.width * 4;
    uint8_t* dst = buf->data + static_cast<size_t>(y) * buf->header.stride;
    for (int x = 0; x < img.width; ++x, src += 4, dst += 4) {
      dst[0] = src[2];  // LVGL keeps B, G, R, A
      dst[1] = src[1];
      dst[2] = src[0];
      dst[3] = src[3];
    }
  }
  return buf;
}

}  // namespace

bool build(lv_obj_t* scr, const char* version) {
  const int32_t h = lv_display_get_vertical_resolution(nullptr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x020D06), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  if (s_portrait == nullptr) s_portrait = decodePortrait();
  if (s_portrait != nullptr) {
    lv_obj_t* img = lv_image_create(scr);
    lv_image_set_src(img, s_portrait);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -h * 20 / 100);
  }
  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "PLANE RADAR");
  lv_obj_set_style_text_color(title, lv_color_hex(0x40FF80), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_40, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, h * 7 / 100);
  s_status = lv_label_create(scr);
  lv_label_set_text(s_status, "");
  lv_obj_set_style_text_color(s_status, lv_color_hex(0x8EA3AE), 0);
  lv_obj_set_style_text_font(s_status, &lv_font_montserrat_22, 0);
  lv_obj_align(s_status, LV_ALIGN_CENTER, 0, h * 15 / 100);
  // The version at the bottom (inside the round panel's edge).
  lv_obj_t* ver = lv_label_create(scr);
  lv_label_set_text_fmt(ver, "v%s", version);
  lv_obj_set_style_text_color(ver, lv_color_hex(0x55707C), 0);
  lv_obj_set_style_text_font(ver, &lv_font_montserrat_22, 0);
  lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -h / 9);
  return s_portrait != nullptr;
}

void setStatus(const char* status) {
  if (s_status != nullptr) lv_label_set_text(s_status, status);
}

void release() {
  s_status = nullptr;
  if (s_portrait == nullptr) return;
  lv_image_cache_drop(s_portrait);
  destroyImageBuf(s_portrait);
  s_portrait = nullptr;
}

}  // namespace ui::boot
