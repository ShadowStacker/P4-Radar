#include "core/viewport.h"

#include <cmath>

namespace core {

void Viewport::setScreen(int width, int height) {
  width_ = width;
  height_ = height;
}

void Viewport::setCenter(const LatLon& c) { proj_.setRef(c); }

void Viewport::setRadiusKm(double r) {
  km_per_px_ = r / radiusPx();
  clampScale();
}

void Viewport::clampScale() {
  const double min_kmpp = kMinRadiusKm / radiusPx();
  const double max_kmpp = kMaxRadiusKm / radiusPx();
  if (km_per_px_ < min_kmpp) km_per_px_ = min_kmpp;
  if (km_per_px_ > max_kmpp) km_per_px_ = max_kmpp;
}

PointF Viewport::project(const LatLon& p) const {
  double x_km, y_km;
  proj_.toKm(p, &x_km, &y_km);
  return {static_cast<float>(cx() + x_km / km_per_px_),
          static_cast<float>(cy() - y_km / km_per_px_)};
}

LatLon Viewport::unproject(float x, float y) const {
  return proj_.fromKm((x - cx()) * km_per_px_, (cy() - y) * km_per_px_);
}

PointF Viewport::kmVectorToPx(double deg, double km) const {
  const double r = degToRad(deg);
  return {static_cast<float>(std::sin(r) * km / km_per_px_),
          static_cast<float>(-std::cos(r) * km / km_per_px_)};
}

void Viewport::panPx(float dx, float dy) { setCenter(unproject(cx() - dx, cy() - dy)); }

void Viewport::zoomAt(double factor, float px, float py) {
  if (factor <= 0) return;
  const LatLon anchor = unproject(px, py);
  km_per_px_ /= factor;
  clampScale();
  // Re-centering shifts the projection's reference latitude a little, so
  // refine twice; the residual drops far below a pixel.
  for (int i = 0; i < 3; ++i) {
    const PointF moved = project(anchor);
    panPx(px - moved.x, py - moved.y);
  }
}

static float dist(PointF a, PointF b) { return std::hypot(a.x - b.x, a.y - b.y); }
static PointF mid(PointF a, PointF b) { return {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f}; }

void Viewport::pinchBegin(PointF a, PointF b) {
  stopAnimation();
  pinching_ = true;
  pinch_start_center_ = center();
  pinch_start_kmpp_ = km_per_px_;
  pinch_start_mid_ = mid(a, b);
  pinch_start_dist_ = dist(a, b);
  if (pinch_start_dist_ < 10.0f) pinch_start_dist_ = 10.0f;
}

void Viewport::pinchUpdate(PointF a, PointF b) {
  if (!pinching_) {
    pinchBegin(a, b);
    return;
  }
  // Re-derive from the gesture start each time so rounding never accumulates:
  // the map point under the starting midpoint stays under the fingers.
  setCenter(pinch_start_center_);
  km_per_px_ = pinch_start_kmpp_;
  const PointF m = mid(a, b);
  zoomAt(dist(a, b) / pinch_start_dist_, pinch_start_mid_.x, pinch_start_mid_.y);
  panPx(m.x - pinch_start_mid_.x, m.y - pinch_start_mid_.y);
}

void Viewport::pinchEnd() { pinching_ = false; }

void Viewport::dragBegin(PointF p, uint32_t now_ms) {
  stopAnimation();
  drag_last_ = p;
  drag_last_ms_ = now_ms;
  velocity_ = {0, 0};
}

void Viewport::dragUpdate(PointF p, uint32_t now_ms) {
  const float dx = p.x - drag_last_.x;
  const float dy = p.y - drag_last_.y;
  panPx(dx, dy);
  const uint32_t dt = now_ms - drag_last_ms_;
  if (dt > 0) {
    velocity_.x = 0.6f * velocity_.x + 0.4f * (dx / dt);
    velocity_.y = 0.6f * velocity_.y + 0.4f * (dy / dt);
  }
  drag_last_ = p;
  drag_last_ms_ = now_ms;
}

void Viewport::dragEnd(uint32_t now_ms) {
  // A finger that paused before lifting should not throw the map.
  if (now_ms - drag_last_ms_ > 80) velocity_ = {0, 0};
  const float speed = std::hypot(velocity_.x, velocity_.y);
  flinging_ = speed > 0.25f;
  fling_last_ms_ = now_ms;
}

void Viewport::flyTo(const LatLon& c, double radius_km, uint32_t now_ms, uint32_t duration_ms) {
  flinging_ = false;
  pinching_ = false;
  fly_from_ = center();
  fly_to_ = c;
  fly_from_kmpp_ = km_per_px_;
  fly_to_kmpp_ = radius_km / radiusPx();
  fly_start_ms_ = now_ms;
  fly_dur_ms_ = duration_ms ? duration_ms : 1;
  flying_ = true;
}

bool Viewport::update(uint32_t now_ms) {
  if (flying_) {
    float t = static_cast<float>(now_ms - fly_start_ms_) / fly_dur_ms_;
    if (t >= 1.0f) {
      t = 1.0f;
      flying_ = false;
    }
    const float e = t * t * (3.0f - 2.0f * t);  // smoothstep
    const double dlon = wrapLon(fly_to_.lon - fly_from_.lon);
    setCenter({fly_from_.lat + (fly_to_.lat - fly_from_.lat) * e, wrapLon(fly_from_.lon + dlon * e)});
    // Zoom in log space so it feels uniform regardless of zoom direction.
    km_per_px_ = std::exp(std::log(fly_from_kmpp_) + (std::log(fly_to_kmpp_) - std::log(fly_from_kmpp_)) * e);
    clampScale();
    return true;
  }
  if (flinging_) {
    const uint32_t dt = now_ms - fling_last_ms_;
    fling_last_ms_ = now_ms;
    if (dt == 0) return true;
    panPx(velocity_.x * dt, velocity_.y * dt);
    const float decay = std::exp(-static_cast<float>(dt) / 220.0f);
    velocity_.x *= decay;
    velocity_.y *= decay;
    if (std::hypot(velocity_.x, velocity_.y) < 0.02f) flinging_ = false;
    return true;
  }
  return false;
}

double niceRingStep(double radius, int target_rings) {
  if (radius <= 0 || target_rings <= 0) return 1.0;
  const double raw = radius / target_rings;
  const double mag = std::pow(10.0, std::floor(std::log10(raw)));
  const double n = raw / mag;
  double nice;
  if (n < 1.5) nice = 1;
  else if (n < 2.25) nice = 2;
  else if (n < 3.5) nice = 2.5;
  else if (n < 7.5) nice = 5;
  else nice = 10;
  return nice * mag;
}

}  // namespace core
