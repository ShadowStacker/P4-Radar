#include "ui/weather_layer.h"

#include <algorithm>
#include <cmath>

#include "app/weather.h"
#include "ui/lv_compat.h"
#include "core/tiles.h"
#include "ui/image_buf.h"

namespace ui {

namespace {

constexpr int kFramesAnimated = 7;     // the last hour, in 10-minute steps
constexpr int kMaxTilesPerFrame = 6;   // coarser tiles beyond this (radar is coarse anyway)
constexpr uint32_t kStepMs = 500;      // animation
constexpr uint32_t kHoldLastMs = 1800;  // pause on the latest frame

bool sameView(const core::Viewport& vp, const core::LatLon& c, double kmpp) {
  return std::fabs(vp.center().lat - c.lat) < 1e-7 && std::fabs(vp.center().lon - c.lon) < 1e-7 &&
         std::fabs(vp.kmPerPx() - kmpp) < kmpp * 1e-6;
}

}  // namespace

void WeatherLayer::create(lv_obj_t*) {}

void WeatherLayer::destroy() {
  clear();
  if (pic_) destroyImageBuf(pic_);
  pic_ = nullptr;
}

void WeatherLayer::clear() {
  for (auto& kv : tiles_) delete kv.second;
  tiles_.clear();
  frames_.clear();
  frame_index_ = -1;
  shown_time_ = 0;
  pic_valid_ = false;
}

bool WeatherLayer::update(const core::Viewport& vp, bool on, bool animate, int opacity, bool busy, uint32_t now_ms) {
  app::weather::setEnabled(on);
  if (!on) {
    const bool had = on_ && pic_valid_;
    if (on_) clear();
    on_ = false;
    return had;
  }
  on_ = true;
  if (opacity != opacity_) {
    opacity_ = opacity;
    pic_dirty_ = true;
  }

  // Frames on show: the last hour, or only the latest.
  std::vector<int64_t> all = app::weather::frames();
  const size_t keep = animate ? kFramesAnimated : 1;
  if (all.size() > keep) all.erase(all.begin(), all.end() - static_cast<long>(keep));
  if (all != frames_) {
    frames_ = all;
    frame_index_ = frames_.empty() ? -1 : static_cast<int>(frames_.size()) - 1;
    pic_dirty_ = true;
  }

  // Tiles on screen: zoom from the scale, coarser until only a few are needed.
  const float W = vp.cx() * 2, H = vp.cy() * 2;
  const core::LatLon nw = vp.unproject(0, 0), se = vp.unproject(W, H);
  int z = core::weather::zoomFor(vp.kmPerPx(), vp.center().lat);
  for (;; --z) {
    tx0_ = core::tiles::tileX(nw.lon, z), tx1_ = core::tiles::tileX(se.lon, z);
    ty0_ = core::tiles::tileY(nw.lat, z), ty1_ = core::tiles::tileY(se.lat, z);
    if (z <= 1 || (tx1_ >= tx0_ && (tx1_ - tx0_ + 1) * (ty1_ - ty0_ + 1) <= kMaxTilesPerFrame)) break;
  }
  if (z != z_) {
    z_ = z;
    pic_dirty_ = true;
  }

  // Adopt finished tiles.
  uint64_t key;
  core::weather::Tile* tile;
  while (app::weather::popReady(&key, &tile)) {
    auto old = tiles_.find(key);
    if (old != tiles_.end()) delete old->second;
    tiles_[key] = tile;
    pic_dirty_ = true;
  }

  // Ask for what's missing (newest frame first) and drop what's not needed.
  std::vector<uint64_t> missing, wanted;
  for (int f = static_cast<int>(frames_.size()) - 1; f >= 0; --f) {
    for (uint32_t ty = ty0_; ty <= ty1_ && tx1_ >= tx0_; ++ty) {
      for (uint32_t tx = tx0_; tx <= tx1_; ++tx) {
        const uint64_t k = core::weather::key(frames_[f], z_, tx, ty);
        wanted.push_back(k);
        if (!tiles_.count(k)) missing.push_back(k);
      }
    }
  }
  app::weather::request(missing);
  if (tiles_.size() > wanted.size() + 12) {
    for (auto it = tiles_.begin(); it != tiles_.end();) {
      if (std::find(wanted.begin(), wanted.end(), it->first) == wanted.end()) {
        delete it->second;
        it = tiles_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Animation: step through the frames whose tiles are all here.
  auto complete = [&](int f) {
    for (uint32_t ty = ty0_; ty <= ty1_; ++ty) {
      for (uint32_t tx = tx0_; tx <= tx1_; ++tx) {
        if (!tiles_.count(core::weather::key(frames_[f], z_, tx, ty))) return false;
      }
    }
    return true;
  };
  if (!frames_.empty() && static_cast<int32_t>(now_ms - next_step_ms_) >= 0) {
    const int n = static_cast<int>(frames_.size());
    int next = frame_index_;
    for (int i = 1; i <= n; ++i) {
      const int f = (frame_index_ + i) % n;
      if (complete(f)) {
        next = f;
        break;
      }
    }
    if (next != frame_index_) {
      frame_index_ = next;
      pic_dirty_ = true;
    }
    next_step_ms_ = now_ms + (frame_index_ == n - 1 ? kHoldLastMs : kStepMs);
  }

  const int64_t want_time = frame_index_ >= 0 ? frames_[frame_index_] : 0;
  const bool view_changed = !pic_valid_ || !sameView(vp, pic_center_, pic_kmpp_);
  if (!busy && (view_changed || pic_dirty_ || want_time != pic_time_)) {
    compose(vp);
    return true;
  }
  return false;
}

void WeatherLayer::compose(const core::Viewport& vp) {
  const int W = static_cast<int>(vp.cx() * 2), H = static_cast<int>(vp.cy() * 2);
  if (pic_ == nullptr) pic_ = createImageBuf(W, H, LV_COLOR_FORMAT_ARGB8888);
  if (pic_ == nullptr) return;
  const int64_t t = frame_index_ >= 0 ? frames_[frame_index_] : 0;
  const core::LatLon c0 = vp.unproject(0, 0), c1 = vp.unproject(1, 1);
  auto* out = reinterpret_cast<uint32_t*>(pic_->data);
  core::weather::compose(out, W, H, c0.lon, c1.lon - c0.lon, c0.lat, c1.lat - c0.lat, z_,
                         [&](uint32_t x, uint32_t y) -> const core::weather::Tile* {
                           if (t == 0) return nullptr;
                           auto it = tiles_.find(core::weather::key(t, z_, x, y));
                           return it == tiles_.end() ? nullptr : it->second;
                         },
                         opacity_);
  lv_image_cache_drop(pic_);
  pic_center_ = vp.center();
  pic_kmpp_ = vp.kmPerPx();
  pic_time_ = t;
  shown_time_ = t;
  pic_valid_ = true;
  pic_dirty_ = false;
}

bool WeatherLayer::draw(lv_layer_t* layer, const core::Viewport& vp) const {
  if (!on_ || !pic_valid_ || pic_ == nullptr || shown_time_ == 0) return false;
  const int32_t W = static_cast<int32_t>(vp.cx() * 2), H = static_cast<int32_t>(vp.cy() * 2);
  lv_draw_image_dsc_t d;
  lv_draw_image_dsc_init(&d);
  d.src = pic_;
  lv_area_t area = {0, 0, W - 1, H - 1};
  if (!sameView(vp, pic_center_, pic_kmpp_)) {
    const core::PointF c = vp.project(pic_center_);
    const float s = static_cast<float>(pic_kmpp_ / vp.kmPerPx());
    d.scale_x = d.scale_y = static_cast<int32_t>(std::lround(s * 256));
    d.pivot.x = W / 2;
    d.pivot.y = H / 2;
    const int32_t x1 = static_cast<int32_t>(std::lround(c.x)) - W / 2;
    const int32_t y1 = static_cast<int32_t>(std::lround(c.y)) - H / 2;
    area = {x1, y1, x1 + W - 1, y1 + H - 1};
  }
  lv_draw_image(layer, &d, &area);
  return true;
}

}  // namespace ui
