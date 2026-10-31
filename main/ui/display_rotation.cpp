// Why not the LVGL adapter's own rotation: on this board its rotated
// partial-refresh path (PPA rotation per stripe plus rotated dirty-area
// repair) froze the UI on busy screens such as Display settings. Its
// unrotated path is the one that has been reliable, so we hand it pixels
// that are already rotated.

#include "ui/display_rotation.h"

#include <cstring>

#include "core/rotation.h"
// lv_display_t internals (flush_cb, buf_act, inv_areas). The header moved
// to the LVGL root in 9.3+; 9.2 only has the src/ one.
#if __has_include("lvgl_private.h")
#include "lvgl_private.h"
#else
#include "src/lvgl_private.h"
#endif
#include "platform/platform.h"

namespace ui {

namespace {

int s_quarter = 0;
lv_display_flush_cb_t s_inner = nullptr;
uint16_t* s_tmp = nullptr;
size_t s_tmp_px = 0;

FlushStats s_stats;

/** No rotation: only the timing. */
void flushTimed(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  const uint64_t t0 = plat::micros();
  s_inner(disp, area, px_map);
  s_stats.flush_us += plat::micros() - t0;
  s_stats.stripes++;
}

void flushRotated(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  const uint64_t t0 = plat::micros();
  const int W = static_cast<int>(disp->hor_res);
  const int H = static_cast<int>(disp->ver_res);
  const int w = lv_area_get_width(area);
  const int h = lv_area_get_height(area);
  lv_draw_buf_t* buf = disp->buf_act;
  const size_t n = static_cast<size_t>(w) * h;

  if (n > s_tmp_px) {
    plat::freeLarge(s_tmp);
    s_tmp = static_cast<uint16_t*>(plat::allocLarge(n * sizeof(uint16_t)));
    s_tmp_px = s_tmp ? n : 0;
  }
  if (s_tmp == nullptr || buf == nullptr || lv_color_format_get_size(static_cast<lv_color_format_t>(buf->header.cf)) != 2) {
    s_inner(disp, area, px_map);  // can't rotate; better a wrong picture than none
    return;
  }

  // Rotate the stripe, then put it back into LVGL's buffer, which the driver
  // expects to read from (LVGL reshapes the buffer before the next stripe).
  const int stride_px = static_cast<int>(buf->header.stride / 2);
  core::rotatePixels(reinterpret_cast<const uint16_t*>(px_map), w, h, stride_px, s_tmp, s_quarter);
  std::memcpy(px_map, s_tmp, n * sizeof(uint16_t));
  s_stats.rotate_us += plat::micros() - t0;
  const bool odd = (s_quarter & 1) != 0;
  buf->header.w = static_cast<uint32_t>(odd ? h : w);
  buf->header.h = static_cast<uint32_t>(odd ? w : h);
  buf->header.stride = static_cast<uint32_t>((odd ? h : w) * 2);

  const core::Rect p = core::uiRectToPanel({area->x1, area->y1, area->x2, area->y2}, s_quarter, W, H);
  const lv_area_t panel_area = {p.x1, p.y1, p.x2, p.y2};

  // On the frame's last stripe the driver reads the dirty areas to repair the
  // other frame buffers; they must be in panel coordinates too. LVGL clears
  // them right after this frame.
  if (lv_display_flush_is_last(disp)) {
    for (uint32_t i = 0; i < disp->inv_p && i < LV_INV_BUF_SIZE; ++i) {
      lv_area_t& a = disp->inv_areas[i];
      const core::Rect r = core::uiRectToPanel({a.x1, a.y1, a.x2, a.y2}, s_quarter, W, H);
      a = {r.x1, r.y1, r.x2, r.y2};
    }
  }
  s_inner(disp, &panel_area, px_map);
  s_stats.flush_us += plat::micros() - t0;
  s_stats.stripes++;
}

}  // namespace

void installDisplayRotation(lv_display_t* disp, int quarter_turns) {
  if (disp == nullptr || s_inner != nullptr || disp->flush_cb == nullptr) return;
  s_inner = disp->flush_cb;
  // The UI layout assumes a square panel for rotation; 0 is only timed.
  if ((quarter_turns & 3) == 0 || disp->hor_res != disp->ver_res) {
    lv_display_set_flush_cb(disp, flushTimed);
    return;
  }
  s_quarter = quarter_turns & 3;
  lv_display_set_flush_cb(disp, flushRotated);
}

FlushStats flushStats() { return s_stats; }

}  // namespace ui
