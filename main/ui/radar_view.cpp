#include "ui/radar_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "app/app.h"
#include "data/airports.h"
#include "platform/platform.h"
#include "ui/image_buf.h"
#include "ui/lv_compat.h"
#include "ui/sprites.h"
#include "ui/theme.h"

namespace ui {

namespace {

constexpr uint32_t kFastFrameMs = 33;   // ~30 fps while animating
constexpr uint32_t kSlowFrameMs = 100;  // 10 fps is plenty for dead-reckoned motion
constexpr float kSweepTailDeg = 70.0f;
constexpr int kSweepSlices = 20;

/** Night theme stays monochrome red to preserve night vision. */
bool useAltitudeColors(const core::Settings& st) {
  return st.color_mode == core::ColorMode::Altitude && st.theme != core::Theme::Night;
}

float distPx(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }

bool areasOverlap(const lv_area_t& a, const lv_area_t& b) {
  return a.x1 <= b.x2 && b.x1 <= a.x2 && a.y1 <= b.y2 && b.y1 <= a.y2;
}

lv_area_t boxAround(float x, float y, float half) {
  return {static_cast<int32_t>(x - half), static_cast<int32_t>(y - half),
          static_cast<int32_t>(x + half), static_cast<int32_t>(y + half)};
}

lv_area_t segBox(float x1, float y1, float x2, float y2, float pad) {
  return {static_cast<int32_t>(std::min(x1, x2) - pad), static_cast<int32_t>(std::min(y1, y2) - pad),
          static_cast<int32_t>(std::max(x1, x2) + pad), static_cast<int32_t>(std::max(y1, y2) + pad)};
}

void formatAltitude(const core::Aircraft& a, char* out, size_t n) {
  if (a.on_ground) {
    std::snprintf(out, n, "GND");
  } else if (!a.alt_valid) {
    std::snprintf(out, n, "---");
  } else if (a.alt_ft >= 18000) {
    std::snprintf(out, n, "FL%03d", static_cast<int>((a.alt_ft + 50) / 100));
  } else {
    std::snprintf(out, n, "%d", static_cast<int>((a.alt_ft + 50) / 100 * 100));
  }
}

const char* displayName(const core::Aircraft& a) {
  if (a.callsign[0]) return a.callsign;
  if (a.reg[0]) return a.reg;
  return a.hex;
}

void drawLine(lv_layer_t* layer, float x1, float y1, float x2, float y2, lv_color_t c, int32_t w,
              lv_opa_t opa, bool round = false) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x1;
  d.p1.y = y1;
  d.p2.x = x2;
  d.p2.y = y2;
  d.color = c;
  d.width = w;
  d.opa = opa;
  d.round_start = d.round_end = round ? 1 : 0;
  lv_draw_line(layer, &d);
}

void drawCircle(lv_layer_t* layer, float x, float y, float r, lv_color_t c, lv_opa_t opa,
                int32_t border = 0) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  if (border > 0) {
    d.bg_opa = LV_OPA_TRANSP;
    d.border_color = c;
    d.border_width = border;
    d.border_opa = opa;
  } else {
    d.bg_color = c;
    d.bg_opa = opa;
  }
  lv_area_t a = boxAround(x, y, r);
  lv_draw_rect(layer, &d, &a);
}

void drawText(lv_layer_t* layer, const char* txt, int32_t x, int32_t y, const lv_font_t* f,
              lv_color_t c, lv_opa_t opa = LV_OPA_COVER, lv_text_align_t align = LV_TEXT_ALIGN_LEFT,
              int32_t w = 0) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.text = txt;
  d.text_local = 1;  // draw tasks run later; callers pass temporary buffers
  d.font = f;
  d.color = c;
  d.opa = opa;
  d.align = align;
  if (w == 0) {
    lv_point_t sz;
    lv_text_get_size(&sz, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    w = sz.x + 2;
  }
  lv_area_t a = {x, y, x + w, y + lv_font_get_line_height(f)};
  lv_draw_label(layer, &d, &a);
}

void drawTextCentered(lv_layer_t* layer, const char* txt, float cx, float cy, const lv_font_t* f,
                      lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
  lv_point_t sz;
  lv_text_get_size(&sz, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  drawText(layer, txt, static_cast<int32_t>(cx - sz.x / 2.0f), static_cast<int32_t>(cy - sz.y / 2.0f),
           f, c, opa, LV_TEXT_ALIGN_LEFT, sz.x + 2);
}

}  // namespace

RadarView& RadarView::get() {
  static RadarView v;
  return v;
}

void RadarView::create(lv_obj_t* parent) {
  const auto& st = app::settings();
  vp_.setScreen(screenW(), screenH());
  if (!vp_initialized_) {  // keep the current view when rebuilding for a theme change
    vp_.setCenter({st.home_lat, st.home_lon});
    vp_.setRadiusKm(st.home_radius_km);
    vp_initialized_ = true;
  }
  query_center_ = vp_.center();
  query_radius_km_ = vp_.radiusKm();
  app::setQueryArea(query_center_, query_radius_km_);

  static_buf_ = createImageBuf(screenW(), screenH(), LV_COLOR_FORMAT_RGB565);
  canvas_ = lv_canvas_create(parent);
  lv_canvas_set_draw_buf(canvas_, static_buf_);
  lv_obj_set_pos(canvas_, 0, 0);
  lv_obj_remove_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);

  overlay_ = lv_obj_create(parent);
  lv_obj_remove_style_all(overlay_);
  lv_obj_set_size(overlay_, screenW(), screenH());
  lv_obj_set_pos(overlay_, 0, 0);
  lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(overlay_, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_add_event_cb(overlay_, onEvent, LV_EVENT_ALL, this);
  lv_obj_add_event_cb(overlay_, onDrawOverlay, LV_EVENT_DRAW_MAIN, this);

  last_frame_ms_ = plat::millis();
  timer_ = lv_timer_create(onTimer, kFastFrameMs, this);
  frame();
}

void RadarView::destroy() {
  if (timer_ != nullptr) lv_timer_delete(timer_);
  timer_ = nullptr;
  canvas_ = nullptr;
  overlay_ = nullptr;
  destroyImageBuf(static_buf_);
  static_buf_ = nullptr;
  gesture_ = Gesture::None;
  interacting_ = false;
  static_dirty_ = true;
}

void RadarView::setPaused(bool paused) {
  if (timer_ == nullptr) return;
  if (paused) {
    lv_timer_pause(timer_);
  } else {
    static_dirty_ = true;
    last_frame_ms_ = plat::millis();
    lv_timer_resume(timer_);
  }
}

// ---------------------------------------------------------------------------
// Public controls

void RadarView::goHome(bool animate) {
  const auto& st = app::settings();
  follow_ = false;
  if (animate) {
    vp_.flyTo({st.home_lat, st.home_lon}, st.home_radius_km, plat::millis(), 800);
    app::sound(core::Sound::Home);
  } else {
    vp_.stopAnimation();
    vp_.setCenter({st.home_lat, st.home_lon});
    vp_.setRadiusKm(st.home_radius_km);
  }
}

void RadarView::zoomBy(double factor) {
  vp_.flyTo(vp_.center(), vp_.radiusKm() / factor, plat::millis(), 250);
}

bool RadarView::atHome() const {
  const auto& st = app::settings();
  const double d = core::distanceKm(vp_.center(), {st.home_lat, st.home_lon});
  const double zr = vp_.radiusKm() / st.home_radius_km;
  return d < vp_.radiusKm() * 0.04 && zr > 0.9 && zr < 1.1;
}

int RadarView::findSelected() const {
  if (selected_hex_.empty()) return -1;
  for (size_t i = 0; i < tracks_.size(); ++i) {
    if (selected_hex_ == tracks_[i].ac.hex) return static_cast<int>(i);
  }
  return -1;
}

const core::Track* RadarView::selected() const {
  const int i = findSelected();
  return i < 0 ? nullptr : &tracks_[i];
}

void RadarView::clearSelection() {
  if (selected_hex_.empty()) return;
  selected_hex_.clear();
  follow_ = false;
  if (on_selection_changed) on_selection_changed();
}

void RadarView::setFollow(bool on) { follow_ = on && findSelected() >= 0; }

void RadarView::setPickMode(bool on) {
  pick_mode_ = on;
  if (on) clearSelection();
}

// ---------------------------------------------------------------------------
// Frame loop

void RadarView::onTimer(lv_timer_t* t) {
  static_cast<RadarView*>(lv_timer_get_user_data(t))->frame();
}

void RadarView::frame() {
  const uint32_t now = plat::millis();
  const uint32_t dt = now - last_frame_ms_;
  last_frame_ms_ = now;
  const auto& st = app::settings();

  const bool animating = vp_.update(now);
  const bool gesturing = gesture_ == Gesture::Drag || gesture_ == Gesture::Pinch;
  const bool busy = animating || gesturing;

  if (app::traffic().version() != tracks_version_) {
    tracks_version_ = app::traffic().snapshot(tracks_);
    if (!selected_hex_.empty() && findSelected() < 0) {
      // Selected aircraft dropped out of the feed.
      selected_hex_.clear();
      follow_ = false;
      if (on_selection_changed) on_selection_changed();
    }
  }

  // Lazy follow: re-center only when the aircraft drifts away from the middle,
  // so the static layer does not have to re-render every frame.
  if (follow_ && !busy) {
    const int i = findSelected();
    if (i < 0) {
      follow_ = false;
    } else {
      const core::LatLon pos = core::positionAt(tracks_[i], now);
      const core::PointF p = vp_.project(pos);
      if (distPx(p.x, p.y, vp_.cx(), vp_.cy()) > vp_.radiusPx() * 0.25f) {
        vp_.flyTo(pos, vp_.radiusKm(), now, 900);
      }
    }
  }

  if (st.sweep_enabled) {
    sweep_deg_ += 360.0f * st.sweep_rpm / 60.0f * dt / 1000.0f;
    if (sweep_deg_ >= 360.0f) {
      sweep_deg_ = std::fmod(sweep_deg_, 360.0f);
      app::sound(core::Sound::Ping);
    }
  }

  const bool view_changed = vp_.center().lat != static_center_.lat ||
                            vp_.center().lon != static_center_.lon || vp_.kmPerPx() != static_kmpp_;
  const bool want_trails = st.show_trails && !busy;
  if (static_dirty_ || view_changed || (want_trails && !static_has_trails_) ||
      (want_trails && tracks_version_ != static_tracks_version_)) {
    renderStatic(want_trails);
  }

  prepareBlips(now);
  placeLabels();
  if (!busy) updateQueryArea(true);
  lv_obj_invalidate(overlay_);

  const uint32_t period = (st.sweep_enabled || busy || interacting_) ? kFastFrameMs : kSlowFrameMs;
  lv_timer_set_period(timer_, period);
}

void RadarView::updateQueryArea(bool refresh) {
  const double r = vp_.radiusKm();
  const double moved = core::distanceKm(vp_.center(), query_center_);
  const double ratio = r / query_radius_km_;
  if (moved < r * 0.15 && ratio > 0.8 && ratio < 1.25) return;
  query_center_ = vp_.center();
  query_radius_km_ = r;
  app::setQueryArea(query_center_, query_radius_km_);
  if (refresh) app::requestRefresh();
}

// ---------------------------------------------------------------------------
// Static layer: background, rings, bezel, runways, airports, trails

void RadarView::renderStatic(bool with_trails) {
  const Palette& p = pal();
  const auto& st = app::settings();
  const float W = static_cast<float>(screenW()), H = static_cast<float>(screenH());
  const float cx = vp_.cx(), cy = vp_.cy(), R = vp_.radiusPx();

  static_dirty_ = false;
  static_center_ = vp_.center();
  static_kmpp_ = vp_.kmPerPx();
  static_has_trails_ = with_trails;
  static_tracks_version_ = tracks_version_;

  lv_canvas_fill_bg(canvas_, p.bg, LV_OPA_COVER);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas_, &layer);

  // Soft radial glow toward the center.
  for (int i = 0; i < 4; ++i) drawCircle(&layer, cx, cy, R * (0.98f - i * 0.22f), p.bg_center, 45);

  const core::PointF home = vp_.project({st.home_lat, st.home_lon});
  const float home_dist = distPx(home.x, home.y, cx, cy);

  // --- Range rings around home --------------------------------------------
  if (st.show_rings) {
    const double view_units = app::kmToUnits(vp_.radiusKm());
    const double step_units = core::niceRingStep(view_units, 4);
    const double step_px = app::unitsToKm(step_units) / vp_.kmPerPx();
    const lv_font_t* f = font(FontSize::S);

    // Crosshair through home.
    drawLine(&layer, 0, home.y, W, home.y, p.grid, sc(1) > 1 ? sc(1) : 1, 120);
    drawLine(&layer, home.x, 0, home.x, H, p.grid, sc(1) > 1 ? sc(1) : 1, 120);

    // Label direction: toward the screen center, or north-east when centered.
    float lx = 0.7071f, ly = -0.7071f;
    if (home_dist > R * 0.15f) {
      lx = (cx - home.x) / home_dist;
      ly = (cy - home.y) / home_dist;
    }

    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = p.grid;
    arc.width = sc(2);
    arc.start_angle = 0;
    arc.end_angle = 360;
    arc.center = {static_cast<int32_t>(home.x), static_cast<int32_t>(home.y)};

    const int first = std::max(1, static_cast<int>(std::floor((home_dist - R) / step_px)));
    for (int k = first; k < first + 60; ++k) {
      const float r = static_cast<float>(k * step_px);
      if (r > home_dist + R) break;
      if (r < home_dist - R || r > 30000.0f) continue;  // ring doesn't cross the screen
      arc.radius = static_cast<uint16_t>(r);
      lv_draw_arc(&layer, &arc);

      char txt[16];
      const double v = k * step_units;
      if (v == std::floor(v)) std::snprintf(txt, sizeof(txt), "%.0f %s", v, app::unitSuffix());
      else std::snprintf(txt, sizeof(txt), "%.1f %s", v, app::unitSuffix());
      const float tx = home.x + lx * r, ty = home.y + ly * r;
      if (distPx(tx, ty, cx, cy) < R - sc(60)) {
        lv_point_t sz;
        lv_text_get_size(&sz, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_draw_rect_dsc_t bg;
        lv_draw_rect_dsc_init(&bg);
        bg.bg_color = p.bg;
        bg.bg_opa = 200;
        bg.radius = sc(6);
        lv_area_t a = {static_cast<int32_t>(tx - sz.x / 2 - sc(6)), static_cast<int32_t>(ty - sz.y / 2 - sc(2)),
                       static_cast<int32_t>(tx + sz.x / 2 + sc(6)), static_cast<int32_t>(ty + sz.y / 2 + sc(2))};
        lv_draw_rect(&layer, &bg, &a);
        drawTextCentered(&layer, txt, tx, ty, f, p.grid_text);
      }
    }
  }

  // --- Runways and airport labels -----------------------------------------
  const double rkm = vp_.radiusKm();
  if (st.show_runways && rkm < 250.0) {
    const double lat_span = rkm * 1.2 / core::kKmPerDegLat;
    const double lon_span = lat_span / std::max(0.05, std::cos(core::degToRad(vp_.center().lat)));
    const bool medium_ok = rkm < 70.0;
    const float rw_w = std::clamp(static_cast<float>(0.05 / vp_.kmPerPx()), 2.0f, static_cast<float>(sc(16)));
    for (size_t i = 0; i < data::kRunwayCount; ++i) {
      const data::Runway& rw = data::kRunways[i];
      const data::Airport& ap = data::kAirports[rw.airport_idx];
      if (ap.cls == 1 && !medium_ok) continue;
      const double alat = ap.lat_e7 * 1e-7, alon = ap.lon_e7 * 1e-7;
      if (std::fabs(alat - vp_.center().lat) > lat_span) continue;
      if (std::fabs(core::wrapLon(alon - vp_.center().lon)) > lon_span) continue;
      const core::PointF a = vp_.project({rw.le_lat_e7 * 1e-7, rw.le_lon_e7 * 1e-7});
      const core::PointF b = vp_.project({rw.he_lat_e7 * 1e-7, rw.he_lon_e7 * 1e-7});
      drawLine(&layer, a.x, a.y, b.x, b.y, p.runway, static_cast<int32_t>(rw_w), LV_OPA_COVER);
    }

    if (st.show_airport_labels && rkm < 150.0) {
      std::vector<core::PointF> placed;
      const lv_font_t* f = font(FontSize::S);
      for (int pass = 0; pass < 2; ++pass) {  // large airports claim space first
        if (pass == 1 && !(rkm < 45.0)) break;
        for (size_t i = 0; i < data::kAirportCount; ++i) {
          const data::Airport& ap = data::kAirports[i];
          if (ap.cls != pass) continue;
          const double alat = ap.lat_e7 * 1e-7, alon = ap.lon_e7 * 1e-7;
          if (std::fabs(alat - vp_.center().lat) > lat_span) continue;
          if (std::fabs(core::wrapLon(alon - vp_.center().lon)) > lon_span) continue;
          const core::PointF a = vp_.project({alat, alon});
          if (distPx(a.x, a.y, cx, cy) > R - sc(70)) continue;
          bool crowded = false;
          for (const auto& q : placed) {
            if (distPx(q.x, q.y, a.x, a.y) < sc(70)) {
              crowded = true;
              break;
            }
          }
          if (crowded) continue;
          placed.push_back(a);
          drawTextCentered(&layer, ap.ident, a.x, a.y + sc(22), f, p.runway_label, 230);
        }
      }
    }
  }

  // --- Trails ----------------------------------------------------------------
  if (with_trails) {
    const int32_t w = sc(2) > 1 ? sc(2) : 1;
    for (const core::Track& t : tracks_) {
      if (t.trail_count < 2) continue;
      core::PointF prev = vp_.project({t.trailAt(0).lat, t.trailAt(0).lon});
      for (int i = 1; i < t.trail_count; ++i) {
        const core::TrailPoint& tp = t.trailAt(i);
        const core::PointF cur = vp_.project({tp.lat, tp.lon});
        if (distPx(prev.x, prev.y, cur.x, cur.y) < 3.0f && i != t.trail_count - 1) continue;
        const bool vis = distPx(cur.x, cur.y, cx, cy) < R + 50 || distPx(prev.x, prev.y, cx, cy) < R + 50;
        if (vis) {
          const lv_color_t c = useAltitudeColors(st)
                                   ? altitudeColor(tp.alt_ft, t.ac.on_ground)
                                   : p.trail;
          const lv_opa_t opa = static_cast<lv_opa_t>(40 + 170 * i / t.trail_count);
          drawLine(&layer, prev.x, prev.y, cur.x, cur.y, c, w, opa, true);
        }
        prev = cur;
      }
    }
  }

  // --- Home marker -----------------------------------------------------------
  drawCircle(&layer, home.x, home.y, sc(9), p.home, 220, sc(2));
  drawCircle(&layer, home.x, home.y, sc(3), p.home, LV_OPA_COVER);

  // --- Compass bezel (screen-fixed, north up) -------------------------------
  {
    const lv_font_t* fc = font(FontSize::L);
    const lv_font_t* fs = font(FontSize::S);
    drawCircle(&layer, cx, cy, R - sc(1), p.bezel, 180, sc(2));
    for (int deg = 0; deg < 360; deg += 5) {
      const float rad = static_cast<float>(core::degToRad(deg));
      const float sx = std::sin(rad), sy = -std::cos(rad);
      const float len = deg % 30 == 0 ? sc(20) : (deg % 10 == 0 ? sc(12) : sc(7));
      const float r0 = R - sc(4);
      drawLine(&layer, cx + sx * r0, cy + sy * r0, cx + sx * (r0 - len), cy + sy * (r0 - len), p.bezel,
               deg % 30 == 0 ? sc(3) : sc(2), deg % 10 == 0 ? 230 : 150);
      if (deg % 30 == 0) {
        const float rt = R - sc(46);
        char txt[8];
        const lv_font_t* f = fs;
        lv_color_t c = p.bezel_text;
        switch (deg) {
          case 0: std::snprintf(txt, sizeof(txt), "N"); f = fc; c = p.text; break;
          case 90: std::snprintf(txt, sizeof(txt), "E"); f = fc; break;
          case 180: std::snprintf(txt, sizeof(txt), "S"); f = fc; break;
          case 270: std::snprintf(txt, sizeof(txt), "W"); f = fc; break;
          default: std::snprintf(txt, sizeof(txt), "%03d", deg); break;
        }
        drawTextCentered(&layer, txt, cx + sx * rt, cy + sy * rt, f, c);
      }
    }
  }

  lv_canvas_finish_layer(canvas_, &layer);
  lv_obj_invalidate(canvas_);
}

// ---------------------------------------------------------------------------
// Per-frame overlay

void RadarView::prepareBlips(uint32_t now) {
  const Palette& p = pal();
  const auto& st = app::settings();
  const float cx = vp_.cx(), cy = vp_.cy(), R = vp_.radiusPx();
  blips_.clear();

  // The beam rotates around home when it's reasonably close, else the screen center.
  const core::PointF home = vp_.project({st.home_lat, st.home_lon});
  sweep_center_ = distPx(home.x, home.y, cx, cy) < R * 2.5f ? home : core::PointF{cx, cy};

  const lv_font_t* f = font(FontSize::S);
  (void)f;
  for (size_t i = 0; i < tracks_.size(); ++i) {
    const core::Track& t = tracks_[i];
    const core::LatLon pos = core::positionAt(t, now);
    const core::PointF s = vp_.project(pos);
    const float d = distPx(s.x, s.y, cx, cy);
    if (d > R * 1.6f) continue;

    Blip b{};
    b.track_idx = static_cast<int>(i);
    b.selected = selected_hex_ == t.ac.hex;
    b.emergency = t.ac.emergency;
    b.kind = static_cast<uint8_t>(sprites::kindFor(t.ac.category, t.ac.on_ground));
    b.heading = t.ac.heading_deg;
    b.color = useAltitudeColors(st) ? altitudeColor(t.ac.alt_ft, t.ac.on_ground)
                                                          : p.aircraft;
    if (b.emergency) b.color = p.danger;
    b.opa = LV_OPA_COVER;

    if (d > R - sc(12)) {
      // Beyond the bezel: a direction cue on the rim, like the original radar.
      const float k = (R - sc(12)) / d;
      b.x = cx + (s.x - cx) * k;
      b.y = cy + (s.y - cy) * k;
      b.kind = 0xFF;  // rim marker
      b.opa = 170;
      blips_.push_back(b);
      continue;
    }
    b.x = s.x;
    b.y = s.y;

    if (st.sweep_enabled && st.sweep_reveal) {
      // Fade the echo as the beam moves away; it brightens again on the next pass.
      float brg = std::atan2(b.x - sweep_center_.x, -(b.y - sweep_center_.y)) * 57.29578f;
      if (brg < 0) brg += 360.0f;
      float since = sweep_deg_ - brg;
      if (since < 0) since += 360.0f;
      b.opa = static_cast<lv_opa_t>(255 - 195 * (since / 360.0f));
      if (b.selected || b.emergency) b.opa = LV_OPA_COVER;
    }

    if (st.show_speed_vectors && t.ac.gs_kt > 30 && !t.ac.on_ground) {
      const core::PointF v = vp_.kmVectorToPx(t.ac.track_deg, t.ac.gs_kt * core::kKmPerNm / 60.0);
      float len = std::hypot(v.x, v.y);
      if (len > 4.0f) {
        const float maxlen = R * 0.35f;
        const float k = len > maxlen ? maxlen / len : 1.0f;
        b.has_vector = true;
        b.vx = b.x + v.x * k;
        b.vy = b.y + v.y * k;
      }
    }

    if (st.show_trails && t.trail_count > 0) {
      const core::TrailPoint& last = t.trailAt(t.trail_count - 1);
      const core::PointF h = vp_.project({last.lat, last.lon});
      if (distPx(h.x, h.y, b.x, b.y) > 2.0f) {
        b.has_head = true;
        b.hx = h.x;
        b.hy = h.y;
      }
    }

    const bool want_label = b.selected || b.emergency || st.labels != core::LabelMode::Off;
    if (want_label) {
      std::snprintf(b.line1, sizeof(b.line1), "%s", displayName(t.ac));
      if (st.labels == core::LabelMode::Full || b.selected || b.emergency) {
        char alt[12];
        formatAltitude(t.ac, alt, sizeof(alt));
        const char* arrow = "";
        if (!t.ac.on_ground && t.ac.vrate_fpm > 400) arrow = " " LV_SYMBOL_UP;
        else if (!t.ac.on_ground && t.ac.vrate_fpm < -400) arrow = " " LV_SYMBOL_DOWN;
        std::snprintf(b.line2, sizeof(b.line2), "%s%s %s", alt, arrow, t.ac.type);
      }
      b.label = true;
    }
    blips_.push_back(b);
  }

  // Draw selected/emergency on top: move them to the end.
  std::stable_partition(blips_.begin(), blips_.end(),
                        [](const Blip& b) { return !(b.selected || b.emergency); });
}

void RadarView::placeLabels() {
  const lv_font_t* f1 = font(FontSize::S);
  const lv_font_t* f2 = font(FontSize::S);
  const int32_t lh = lv_font_get_line_height(f1);
  const float cx = vp_.cx(), cy = vp_.cy(), R = vp_.radiusPx();
  std::vector<lv_area_t> placed;
  placed.reserve(blips_.size() * 2);
  // Symbols are obstacles too, so labels don't cover other aircraft.
  for (const Blip& b : blips_) {
    if (b.kind == 0xFF) continue;
    placed.push_back(boxAround(b.x, b.y, sprites::size(static_cast<sprites::Kind>(b.kind)) * 0.4f));
  }

  // Selected and emergency labels get first pick of positions.
  for (int pass = 0; pass < 2; ++pass) {
    for (Blip& b : blips_) {
      if (!b.label || b.kind == 0xFF) continue;
      const bool prio = b.selected || b.emergency;
      if ((pass == 0) != prio) continue;
      lv_point_t s1, s2 = {0, 0};
      lv_text_get_size(&s1, b.line1, f1, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      if (b.line2[0]) lv_text_get_size(&s2, b.line2, f2, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      const int32_t w = std::max(s1.x, s2.x) + 2;
      const int32_t h = b.line2[0] ? lh * 2 : lh;
      const int32_t half = sprites::size(static_cast<sprites::Kind>(b.kind)) / 2 + sc(4);
      const int32_t bx = static_cast<int32_t>(b.x), by = static_cast<int32_t>(b.y);
      const lv_area_t cand[4] = {
          {bx + half, by - h / 2, bx + half + w, by + h / 2},          // right
          {bx - half - w, by - h / 2, bx - half, by + h / 2},          // left
          {bx - w / 2, by + half, bx + w / 2, by + half + h},          // below
          {bx - w / 2, by - half - h, bx + w / 2, by - half},          // above
      };
      b.label = false;
      for (const lv_area_t& a : cand) {
        // Keep labels inside the bezel ring.
        const float far = std::max({distPx(a.x1, a.y1, cx, cy), distPx(a.x2, a.y1, cx, cy),
                                    distPx(a.x1, a.y2, cx, cy), distPx(a.x2, a.y2, cx, cy)});
        if (far > R - sc(26)) continue;
        bool hit = false;
        for (const lv_area_t& o : placed) {
          if (areasOverlap(a, o)) {
            hit = true;
            break;
          }
        }
        if (hit) continue;
        b.label = true;
        b.label_area = a;
        placed.push_back(a);
        break;
      }
      if (!b.label && prio) {
        b.label = true;  // always show the selected one, even if it overlaps
        b.label_area = cand[0];
      }
    }
  }
}

void RadarView::onDrawOverlay(lv_event_t* e) {
  static_cast<RadarView*>(lv_event_get_user_data(e))->drawOverlay(lv_event_get_layer(e));
}

void RadarView::drawSweep(lv_layer_t* layer) {
  const Palette& p = pal();
  const float sx = sweep_center_.x, sy = sweep_center_.y;
  const float big = distPx(sx, sy, vp_.cx(), vp_.cy()) + vp_.radiusPx() + 20.0f;
  const lv_area_t& clip = layerClip(layer);

  lv_draw_triangle_dsc_t tri;
  lv_draw_triangle_dsc_init(&tri);
  const float slice = kSweepTailDeg / kSweepSlices;
  for (int i = 0; i < kSweepSlices; ++i) {
    const float a0 = static_cast<float>(core::degToRad(sweep_deg_ - kSweepTailDeg + i * slice));
    const float a1 = static_cast<float>(core::degToRad(sweep_deg_ - kSweepTailDeg + (i + 1) * slice + 0.3f));
    const float x0 = sx + std::sin(a0) * big, y0 = sy - std::cos(a0) * big;
    const float x1 = sx + std::sin(a1) * big, y1 = sy - std::cos(a1) * big;
    const lv_area_t box = {static_cast<int32_t>(std::min({sx, x0, x1})), static_cast<int32_t>(std::min({sy, y0, y1})),
                           static_cast<int32_t>(std::max({sx, x0, x1})), static_cast<int32_t>(std::max({sy, y0, y1}))};
    if (!areasOverlap(box, clip)) continue;
    const float k = static_cast<float>(i + 1) / kSweepSlices;
    triangleDscSetColor(&tri, p.sweep, static_cast<lv_opa_t>(k * k * 90));
    tri.p[0].x = sx;
    tri.p[0].y = sy;
    tri.p[1].x = x0;
    tri.p[1].y = y0;
    tri.p[2].x = x1;
    tri.p[2].y = y1;
    lv_draw_triangle(layer, &tri);
  }
  const float a = static_cast<float>(core::degToRad(sweep_deg_));
  const float ex = sx + std::sin(a) * big, ey = sy - std::cos(a) * big;
  if (areasOverlap(segBox(sx, sy, ex, ey, sc(8)), clip)) {
    drawLine(layer, sx, sy, ex, ey, p.sweep, sc(10), 40);
    drawLine(layer, sx, sy, ex, ey, p.sweep, sc(3), 230);
  }
}

void RadarView::drawOverlay(lv_layer_t* layer) {
  const Palette& p = pal();
  const auto& st = app::settings();
  const lv_area_t& clip = layerClip(layer);
  const uint32_t now = plat::millis();

  if (st.sweep_enabled) drawSweep(layer);

  // Trail heads and speed vectors under the symbols.
  for (const Blip& b : blips_) {
    if (b.kind == 0xFF) continue;
    if (b.has_head && areasOverlap(segBox(b.hx, b.hy, b.x, b.y, 4), clip)) {
      drawLine(layer, b.hx, b.hy, b.x, b.y, b.color, sc(2) > 1 ? sc(2) : 1, 200, true);
    }
    if (b.has_vector && areasOverlap(segBox(b.x, b.y, b.vx, b.vy, 4), clip)) {
      drawLine(layer, b.x, b.y, b.vx, b.vy, p.vector, sc(2) > 1 ? sc(2) : 1,
               static_cast<lv_opa_t>(b.opa * 150 / 255), true);
    }
  }

  const lv_font_t* f1 = font(FontSize::S);
  const int32_t lh = lv_font_get_line_height(f1);
  for (const Blip& b : blips_) {
    if (b.kind == 0xFF) {
      if (areasOverlap(boxAround(b.x, b.y, sc(8)), clip)) {
        drawCircle(layer, b.x, b.y, sc(5), b.color, b.opa);
      }
      continue;
    }
    const auto kind = static_cast<sprites::Kind>(b.kind);
    const int32_t sz = sprites::size(kind);
    const lv_area_t box = boxAround(b.x, b.y, sz / 2.0f + sc(16));
    if (areasOverlap(box, clip)) {
      if (b.emergency) {
        const float pulse = 0.5f + 0.5f * std::sin(now * 0.008f);
        drawCircle(layer, b.x, b.y, sz * 0.8f + pulse * sc(8), p.danger, static_cast<lv_opa_t>(120 + 120 * pulse), sc(3));
      }
      if (b.selected) {
        drawCircle(layer, b.x, b.y, sz * 0.78f, p.select, LV_OPA_COVER, sc(3));
      }
      const lv_draw_buf_t* img = sprites::get(kind, b.heading);
      if (img != nullptr) {
        lv_draw_image_dsc_t d;
        lv_draw_image_dsc_init(&d);
        d.src = img;
        d.recolor = b.color;
        d.recolor_opa = LV_OPA_COVER;
        d.opa = b.opa;
        const int32_t x0 = static_cast<int32_t>(std::lround(b.x)) - sz / 2;
        const int32_t y0 = static_cast<int32_t>(std::lround(b.y)) - sz / 2;
        lv_area_t a = {x0, y0, x0 + sz - 1, y0 + sz - 1};
        lv_draw_image(layer, &d, &a);
      }
    }
    if (b.label && areasOverlap(b.label_area, clip)) {
      const lv_color_t c1 = b.emergency ? p.danger : (b.selected ? p.select : p.label);
      drawText(layer, b.line1, b.label_area.x1, b.label_area.y1, f1, c1, b.opa);
      if (b.line2[0]) {
        drawText(layer, b.line2, b.label_area.x1, b.label_area.y1 + lh, f1, p.label_dim,
                 static_cast<lv_opa_t>(b.opa * 230 / 255));
      }
    }
  }

  if (pick_mode_) {
    const float cx = vp_.cx(), cy = vp_.cy();
    drawLine(layer, cx - sc(40), cy, cx - sc(10), cy, p.accent, sc(3), LV_OPA_COVER);
    drawLine(layer, cx + sc(10), cy, cx + sc(40), cy, p.accent, sc(3), LV_OPA_COVER);
    drawLine(layer, cx, cy - sc(40), cx, cy - sc(10), p.accent, sc(3), LV_OPA_COVER);
    drawLine(layer, cx, cy + sc(10), cx, cy + sc(40), p.accent, sc(3), LV_OPA_COVER);
    drawCircle(layer, cx, cy, sc(4), p.accent, LV_OPA_COVER);
  }
}

// ---------------------------------------------------------------------------
// Touch

void RadarView::onEvent(lv_event_t* e) {
  auto* self = static_cast<RadarView*>(lv_event_get_user_data(e));
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED: self->handlePress(e); break;
    case LV_EVENT_PRESSING: self->handlePressing(e); break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: self->handleRelease(e); break;
    case LV_EVENT_LONG_PRESSED:
      if (self->gesture_ == Gesture::Undecided && !self->pick_mode_) {
        self->gesture_ = Gesture::None;
        if (self->on_long_press) {
          self->on_long_press(self->vp_.unproject(static_cast<float>(self->press_pt_.x),
                                                  static_cast<float>(self->press_pt_.y)));
        }
      }
      break;
    default: break;
  }
}

void RadarView::handlePress(lv_event_t* e) {
  (void)e;
  if (app::userActivity()) {
    gesture_ = Gesture::None;  // the touch only wakes the dimmed screen
    return;
  }
  lv_indev_get_point(lv_indev_active(), &press_pt_);
  press_ms_ = plat::millis();
  gesture_ = Gesture::Undecided;
  interacting_ = true;
  vp_.stopAnimation();
  lv_timer_set_period(timer_, kFastFrameMs);
}

void RadarView::handlePressing(lv_event_t* e) {
  (void)e;
  const uint32_t now = plat::millis();
  plat::TouchPoint pts[5];
  const int n = plat::touchPoints(pts, 5);
  if (n >= 2 && gesture_ != Gesture::None) {
    const core::PointF a{static_cast<float>(pts[0].x), static_cast<float>(pts[0].y)};
    const core::PointF b{static_cast<float>(pts[1].x), static_cast<float>(pts[1].y)};
    if (gesture_ != Gesture::Pinch) {
      vp_.pinchBegin(a, b);
      gesture_ = Gesture::Pinch;
      follow_ = false;
    } else {
      vp_.pinchUpdate(a, b);
    }
    return;
  }
  if (gesture_ == Gesture::Pinch) {
    // One finger lifted: finish the pinch but don't turn the rest into a drag,
    // which would make the map jump.
    vp_.pinchEnd();
    gesture_ = Gesture::PinchDone;
    return;
  }
  if (gesture_ != Gesture::Undecided && gesture_ != Gesture::Drag) return;

  lv_point_t pt;
  lv_indev_get_point(lv_indev_active(), &pt);
  const core::PointF p{static_cast<float>(pt.x), static_cast<float>(pt.y)};
  if (gesture_ == Gesture::Undecided) {
    if (distPx(p.x, p.y, press_pt_.x, press_pt_.y) < sc(14)) return;
    gesture_ = Gesture::Drag;
    follow_ = false;
    vp_.dragBegin({static_cast<float>(press_pt_.x), static_cast<float>(press_pt_.y)}, now);
  }
  vp_.dragUpdate(p, now);
}

void RadarView::handleRelease(lv_event_t* e) {
  (void)e;
  const uint32_t now = plat::millis();
  switch (gesture_) {
    case Gesture::Drag: vp_.dragEnd(now); break;
    case Gesture::Pinch: vp_.pinchEnd(); break;
    case Gesture::Undecided:
      if (now - press_ms_ < 450) handleTap(press_pt_);
      break;
    default: break;
  }
  gesture_ = Gesture::None;
  interacting_ = false;
}

void RadarView::handleTap(lv_point_t p) {
  const uint32_t now = plat::millis();
  if (pick_mode_) return;

  // Double tap: zoom in around the tapped point.
  if (now - last_tap_ms_ < 350 &&
      distPx(p.x, p.y, last_tap_pt_.x, last_tap_pt_.y) < sc(50)) {
    last_tap_ms_ = 0;
    vp_.flyTo(vp_.unproject(static_cast<float>(p.x), static_cast<float>(p.y)), vp_.radiusKm() / 2.0,
              now, 350);
    return;
  }
  last_tap_ms_ = now;
  last_tap_pt_ = p;

  int best = -1;
  float best_d = static_cast<float>(sc(44));
  for (size_t i = 0; i < blips_.size(); ++i) {
    if (blips_[i].kind == 0xFF) continue;
    const float d = distPx(blips_[i].x, blips_[i].y, p.x, p.y);
    if (d < best_d) {
      best_d = d;
      best = static_cast<int>(i);
    }
  }
  if (best >= 0) {
    const std::string hex = tracks_[blips_[best].track_idx].ac.hex;
    if (hex != selected_hex_) {
      selected_hex_ = hex;
      follow_ = false;
      app::sound(core::Sound::Click);
      if (on_selection_changed) on_selection_changed();
    }
  } else {
    clearSelection();
  }
}

}  // namespace ui
