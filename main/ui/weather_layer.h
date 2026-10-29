#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "core/viewport.h"
#include "core/weather.h"
#include "lvgl.h"

namespace ui {

/**
 * Precipitation radar over the map. Keeps the tiles of the last hour's
 * frames, draws the current frame into a screen-sized transparent picture
 * (when the view settles and as the animation steps), and draws that picture
 * over the map: shifted and scaled during pans and pinches, like MapLayer.
 */
class WeatherLayer {
 public:
  void create(lv_obj_t* parent);
  void destroy();

  /**
   * Once per frame. `on`, `animate` and `opacity` come from the settings.
   * Returns true when the picture changed (the static layer must redraw).
   */
  bool update(const core::Viewport& vp, bool on, bool animate, int opacity, bool busy, uint32_t now_ms);
  bool draw(lv_layer_t* layer, const core::Viewport& vp) const;

  bool active() const { return on_; }
  /** Time (unix s) of the frame shown, 0 if none. */
  int64_t shownTime() const { return shown_time_; }

 private:
  void clear();
  void compose(const core::Viewport& vp);

  bool on_ = false;
  int opacity_ = 70;
  std::unordered_map<uint64_t, core::weather::Tile*> tiles_;
  std::vector<int64_t> frames_;  // the ones on show (oldest first)
  int z_ = 0;
  uint32_t tx0_ = 0, tx1_ = 0, ty0_ = 0, ty1_ = 0;  // tile range on screen at z_
  int frame_index_ = -1;                             // into frames_
  uint32_t next_step_ms_ = 0;
  int64_t shown_time_ = 0;

  lv_draw_buf_t* pic_ = nullptr;
  bool pic_valid_ = false;
  bool pic_dirty_ = false;
  int64_t pic_time_ = 0;
  core::LatLon pic_center_;
  double pic_kmpp_ = 0;
};

}  // namespace ui
