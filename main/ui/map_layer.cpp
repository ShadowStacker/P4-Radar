#include "ui/map_layer.h"

#include <algorithm>
#include <cmath>

#include "platform/platform.h"
// lv_image_cache_drop(): its header moved between LVGL 9.2 and 9.6 and isn't
// always part of lvgl.h.
#if __has_include("src/misc/cache/instance/lv_image_cache.h")
#include "src/misc/cache/instance/lv_image_cache.h"
#elif __has_include("src/misc/cache/lv_image_cache.h")
#include "src/misc/cache/lv_image_cache.h"
#endif
#include "ui/image_buf.h"
#include "ui/theme.h"

namespace ui {

namespace {

constexpr size_t kMaxTiles = 40;            // decoded tiles kept (128 KB each)
constexpr int kMaxAdoptPerFrame = 4;        // keep frames short while tiles stream in
constexpr uint32_t kComposeEveryMs = 250;   // while tiles are still arriving...
constexpr uint32_t kComposeEveryManyMs = 800;  // ...when the view needs many (zoomed out)
constexpr int kFallbackLevels = 4;          // draw a parent tile while a tile loads

bool sameView(const core::Viewport& vp, const core::LatLon& c, double kmpp) {
  return vp.center().lat == c.lat && vp.center().lon == c.lon && vp.kmPerPx() == kmpp;
}

bool intersect(const lv_area_t& a, const lv_area_t& b, lv_area_t* out) {
  out->x1 = std::max(a.x1, b.x1);
  out->y1 = std::max(a.y1, b.y1);
  out->x2 = std::min(a.x2, b.x2);
  out->y2 = std::min(a.y2, b.y2);
  return out->x1 <= out->x2 && out->y1 <= out->y2;
}

}  // namespace

void MapLayer::create(lv_obj_t* parent) {
  canvas_ = lv_canvas_create(parent);
  lv_obj_add_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
  pic_valid_ = false;
}

void MapLayer::destroy() {
  clearTiles();
  if (pic_) destroyImageBuf(pic_);
  pic_ = nullptr;
  pic_valid_ = false;
  canvas_ = nullptr;  // deleted with its screen
  look_ = app::maps::Look{};
  last_request_.clear();
}

void MapLayer::clearTiles() {
  for (auto& kv : tiles_) {
    lv_image_cache_drop(kv.second.buf);
    delete kv.second.buf;
    plat::freeLarge(kv.second.pixels);
  }
  tiles_.clear();
}

lv_area_t MapLayer::tileRect(const core::Viewport& vp, uint64_t key) const {
  core::tiles::Variant v;
  int z;
  uint32_t x, y;
  core::tiles::unpack(key, &v, &z, &x, &y);
  const core::PointF nw = vp.project({core::tiles::tileLat(y, z), core::tiles::tileLon(x, z)});
  const core::PointF se = vp.project({core::tiles::tileLat(y + 1.0, z), core::tiles::tileLon(x + 1, z)});
  return {static_cast<int32_t>(std::floor(nw.x)), static_cast<int32_t>(std::floor(nw.y)),
          static_cast<int32_t>(std::ceil(se.x)) - 1, static_cast<int32_t>(std::ceil(se.y)) - 1};
}

void MapLayer::computeNeeded(const core::Viewport& vp) {
  needed_.clear();
  const float W = vp.cx() * 2, H = vp.cy() * 2, R = vp.radiusPx();
  const int z = core::tiles::zoomFor(look_.variant, vp.kmPerPx(), vp.center().lat);
  const core::LatLon nw = vp.unproject(0, 0), se = vp.unproject(W, H);
  const uint32_t x0 = core::tiles::tileX(nw.lon, z), x1 = core::tiles::tileX(se.lon, z);
  const uint32_t y0 = core::tiles::tileY(nw.lat, z), y1 = core::tiles::tileY(se.lat, z);
  if (x1 < x0 || y1 < y0 || (x1 - x0 + 1) * (y1 - y0 + 1) > 120) return;  // date line or absurd zoom
  for (uint32_t ty = y0; ty <= y1; ++ty) {
    for (uint32_t tx = x0; tx <= x1; ++tx) {
      const uint64_t k = core::tiles::key(look_.variant, z, tx, ty);
      const lv_area_t r = tileRect(vp, k);
      // Distance from the screen center to the nearest point of the tile.
      const float dx = std::max({static_cast<float>(r.x1) - vp.cx(), 0.0f, vp.cx() - static_cast<float>(r.x2)});
      const float dy = std::max({static_cast<float>(r.y1) - vp.cy(), 0.0f, vp.cy() - static_cast<float>(r.y2)});
      const float d = std::hypot(dx, dy);
      if (d > R) continue;  // outside the round display
      needed_.push_back({k, d});
    }
  }
  std::sort(needed_.begin(), needed_.end(), [](const Needed& a, const Needed& b) { return a.dist < b.dist; });
}

bool MapLayer::update(const core::Viewport& vp, const app::maps::Look& look, bool busy, lv_color_t bg) {
  ++frame_;
  if (look != look_) {
    clearTiles();
    look_ = look;
    pic_valid_ = false;
    pic_dirty_ = true;
    last_request_.clear();
    app::maps::request(look_, {});  // drops tiles in flight for the old look
  }
  if (!active()) {
    const bool had = pic_valid_;
    pic_valid_ = false;
    return had;
  }

  // Adopt finished tiles.
  uint64_t key;
  uint16_t* pixels;
  for (int i = 0; i < kMaxAdoptPerFrame && app::maps::popReady(&key, &pixels); ++i) {
    auto* buf = new lv_draw_buf_t{};
    const uint32_t stride = core::tiles::kTileSize * 2;
    lv_draw_buf_init(buf, core::tiles::kTileSize, core::tiles::kTileSize, LV_COLOR_FORMAT_RGB565, stride, pixels,
                     stride * core::tiles::kTileSize);
    auto old = tiles_.find(key);
    if (old != tiles_.end()) {
      lv_image_cache_drop(old->second.buf);
      delete old->second.buf;
      plat::freeLarge(old->second.pixels);
    }
    tiles_[key] = {buf, pixels, frame_};
    pic_dirty_ = true;
  }

  // What the current view needs, and what to ask for.
  computeNeeded(vp);
  std::vector<uint64_t> missing;
  for (const Needed& n : needed_) {
    auto it = tiles_.find(n.key);
    if (it != tiles_.end()) {
      it->second.used = frame_;
      continue;
    }
    missing.push_back(n.key);
    // Keep the parent that stands in for it until it arrives.
    for (int up = 1; up <= kFallbackLevels; ++up) {
      auto p = tiles_.find(core::tiles::parentKey(n.key, up));
      if (p != tiles_.end()) {
        p->second.used = frame_;
        break;
      }
    }
  }
  if (missing != last_request_) {
    app::maps::request(look_, missing);
    last_request_ = missing;
  }

  // Evict the least recently needed tiles beyond the budget.
  while (tiles_.size() > kMaxTiles) {
    auto victim = tiles_.end();
    for (auto it = tiles_.begin(); it != tiles_.end(); ++it) {
      if (victim == tiles_.end() || it->second.used < victim->second.used) victim = it;
    }
    if (victim == tiles_.end() || victim->second.used == frame_) break;  // everything is on screen
    lv_image_cache_drop(victim->second.buf);
    delete victim->second.buf;
    plat::freeLarge(victim->second.pixels);
    tiles_.erase(victim);
  }

  // Recompose when the view has settled (or new tiles came in), not on every
  // frame of a gesture: draw() shifts and scales the last picture meanwhile.
  const uint32_t now = plat::millis();
  const bool view_changed = !pic_valid_ || !sameView(vp, pic_center_, pic_kmpp_);
  const uint32_t every = needed_.size() > 16 ? kComposeEveryManyMs : kComposeEveryMs;
  if (!busy && (view_changed || pic_dirty_) && (view_changed || now - last_compose_ms_ >= every)) {
    compose(vp, bg);
    last_compose_ms_ = now;
    return true;
  }
  return false;
}

void MapLayer::drawTile(lv_layer_t* layer, const core::Viewport& vp, uint64_t key, const lv_area_t& clip) const {
  auto it = tiles_.find(key);
  if (it == tiles_.end()) return;
  core::tiles::Variant v;
  int z;
  uint32_t x, y;
  core::tiles::unpack(key, &v, &z, &x, &y);
  const int T = core::tiles::kTileSize;
  const float xl = vp.project({0, core::tiles::tileLon(x, z)}).x;
  const float xr = vp.project({0, core::tiles::tileLon(x + 1, z)}).x;
  // Mercator rows aren't evenly spaced in latitude: at small scales draw the
  // tile in horizontal strips, each placed by its own latitudes.
  const int strips = z >= 6 ? 1 : (z == 5 ? 2 : 4);  // keeps the error around 1-2 px
  for (int s = 0; s < strips; ++s) {
    const double ry0 = static_cast<double>(s) / strips, ry1 = static_cast<double>(s + 1) / strips;
    const float yt = vp.project({core::tiles::tileLat(y + ry0, z), 0}).y;
    const float yb = vp.project({core::tiles::tileLat(y + ry1, z), 0}).y;
    lv_area_t strip = {static_cast<int32_t>(std::floor(xl)), static_cast<int32_t>(std::floor(yt)),
                       static_cast<int32_t>(std::ceil(xr)) - 1, static_cast<int32_t>(std::ceil(yb)) - 1};
    // Never widen the layer's own clip: tiles reach past the screen edges.
    lv_area_t c, c2;
    if (!intersect(strip, clip, &c) || !intersect(c, layer->_clip_area, &c2)) continue;
    c = c2;
    const float sx = (xr - xl) / T;
    const float sy = (yb - yt) / (static_cast<float>(T) / strips);

    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = it->second.buf;
    d.scale_x = static_cast<int32_t>(std::lround(sx * 256));
    d.scale_y = static_cast<int32_t>(std::lround(sy * 256));
    d.pivot.x = 0;
    d.pivot.y = 0;
    d.antialias = 1;
    const int32_t ix = static_cast<int32_t>(std::lround(xl));
    const int32_t iy = static_cast<int32_t>(std::lround(yt - ry0 * T * sy));
    lv_area_t area = {ix, iy, ix + T - 1, iy + T - 1};
    const lv_area_t saved = layer->_clip_area;
    layer->_clip_area = c;
    lv_draw_image(layer, &d, &area);
    layer->_clip_area = saved;
  }
}

void MapLayer::compose(const core::Viewport& vp, lv_color_t bg) {
  const int32_t W = static_cast<int32_t>(vp.cx() * 2), H = static_cast<int32_t>(vp.cy() * 2);
  if (pic_ == nullptr) pic_ = createImageBuf(W, H, LV_COLOR_FORMAT_RGB565);
  if (pic_ == nullptr || canvas_ == nullptr) return;
  lv_canvas_set_draw_buf(canvas_, pic_);
  lv_canvas_fill_bg(canvas_, bg, LV_OPA_COVER);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas_, &layer);
  for (const Needed& n : needed_) {
    const lv_area_t rect = tileRect(vp, n.key);
    if (tiles_.count(n.key)) {
      drawTile(&layer, vp, n.key, rect);
      continue;
    }
    // Not loaded yet: stretch the closest loaded parent over its area.
    for (int up = 1; up <= kFallbackLevels; ++up) {
      const uint64_t parent = core::tiles::parentKey(n.key, up);
      if (tiles_.count(parent)) {
        drawTile(&layer, vp, parent, rect);
        break;
      }
    }
  }
  lv_canvas_finish_layer(canvas_, &layer);
  pic_center_ = vp.center();
  pic_kmpp_ = vp.kmPerPx();
  pic_valid_ = true;
  pic_dirty_ = false;
}

bool MapLayer::draw(lv_layer_t* layer, const core::Viewport& vp) const {
  if (!active() || !pic_valid_ || pic_ == nullptr) return false;
  const int32_t W = static_cast<int32_t>(vp.cx() * 2), H = static_cast<int32_t>(vp.cy() * 2);
  lv_draw_image_dsc_t d;
  lv_draw_image_dsc_init(&d);
  d.src = pic_;
  lv_area_t area = {0, 0, W - 1, H - 1};
  if (!sameView(vp, pic_center_, pic_kmpp_)) {
    // Mid-gesture: move/scale the last picture to where it belongs now.
    const core::PointF c = vp.project(pic_center_);
    const float s = static_cast<float>(pic_kmpp_ / vp.kmPerPx());
    d.scale_x = d.scale_y = static_cast<int32_t>(std::lround(s * 256));
    d.antialias = 0;  // only during the gesture: speed over smoothness
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
