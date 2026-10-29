#pragma once

#include <cstdint>

#include "core/geo.h"

namespace core {

struct PointF {
  float x = 0;
  float y = 0;
};

/**
 * Maps geographic coordinates to screen pixels for a north-up map, and owns
 * all the pan / pinch-zoom / fly-to state. Pure math, no LVGL.
 *
 * Scale is stored as km per pixel; "view radius" is the distance from the
 * screen center to the edge of the round display.
 */
class Viewport {
 public:
  static constexpr double kMinRadiusKm = 1.0;
  static constexpr double kMaxRadiusKm = 460.0;  // adsb.fi caps queries at 250 nm

  void setScreen(int width, int height);
  int width() const { return width_; }
  int height() const { return height_; }
  float cx() const { return width_ * 0.5f; }
  float cy() const { return height_ * 0.5f; }
  /** Radius of the visible circle in px (round panel). */
  float radiusPx() const { return (width_ < height_ ? width_ : height_) * 0.5f; }

  const LatLon& center() const { return proj_.ref; }
  double kmPerPx() const { return km_per_px_; }
  double radiusKm() const { return km_per_px_ * radiusPx(); }

  void setCenter(const LatLon& c);
  void setRadiusKm(double r);

  PointF project(const LatLon& p) const;
  LatLon unproject(float x, float y) const;
  /** Projects a vector of `km` length on bearing `deg` to a pixel delta. */
  PointF kmVectorToPx(double deg, double km) const;

  /** Moves the map so content follows a finger that moved by (dx, dy) px. */
  void panPx(float dx, float dy);
  /** Zooms by `factor` (>1 = zoom in) keeping the point under (px, py) fixed. */
  void zoomAt(double factor, float px, float py);

  // --- Gestures -----------------------------------------------------------
  /** Starts a two-finger pinch; positions in screen px. */
  void pinchBegin(PointF a, PointF b);
  /** Updates an in-progress pinch from the current finger positions. */
  void pinchUpdate(PointF a, PointF b);
  void pinchEnd();
  bool pinching() const { return pinching_; }

  /** Begins a one-finger drag (cancels fling/fly-to). */
  void dragBegin(PointF p, uint32_t now_ms);
  void dragUpdate(PointF p, uint32_t now_ms);
  /** Ends a drag; keeps gliding with the release velocity if fast enough. */
  void dragEnd(uint32_t now_ms);

  // --- Animation ----------------------------------------------------------
  /** Animates to a center/radius over duration_ms (ease-in-out). */
  void flyTo(const LatLon& c, double radius_km, uint32_t now_ms, uint32_t duration_ms = 700);
  /** Advances fly-to and fling. Returns true while the view is still moving. */
  bool update(uint32_t now_ms);
  bool animating() const { return flying_ || flinging_ || zoom_rate_ != 0; }
  void stopAnimation() {
    flying_ = flinging_ = false;
    zoom_rate_ = 0;
  }

  /**
   * Animated zoom by `factor` (>1 = in) about the center. Steps in quick
   * succession add up: each starts from where the previous one is heading.
   */
  void zoomStep(double factor, uint32_t now_ms, uint32_t duration_ms = 220);
  /**
   * Continuous zoom (a held button): `octaves_per_s` > 0 zooms in, < 0 out,
   * 0 stops. Eases in over the first 300 ms.
   */
  void setZoomRate(double octaves_per_s, uint32_t now_ms);
  double targetRadiusKm() const { return (flying_ ? fly_to_kmpp_ : km_per_px_) * radiusPx(); }

 private:
  void clampScale();

  int width_ = 800;
  int height_ = 800;
  LocalProjection proj_;
  double km_per_px_ = 0.05;

  // pinch
  bool pinching_ = false;
  LatLon pinch_start_center_;
  double pinch_start_kmpp_ = 0;
  PointF pinch_start_mid_;
  float pinch_start_dist_ = 1;

  // drag + fling
  PointF drag_last_;
  uint32_t drag_last_ms_ = 0;
  PointF velocity_;  // px per ms (smoothed)
  bool flinging_ = false;
  uint32_t fling_last_ms_ = 0;

  // fly-to
  bool flying_ = false;
  LatLon fly_from_, fly_to_;
  double fly_from_kmpp_ = 0, fly_to_kmpp_ = 0;
  uint32_t fly_start_ms_ = 0, fly_dur_ms_ = 1;

  // continuous zoom
  double zoom_rate_ = 0;  // octaves per second
  uint32_t zoom_start_ms_ = 0, zoom_last_ms_ = 0;
};

/** Picks a "nice" ring spacing (1/2/2.5/5 x 10^n) so ~`target_rings` fit in `radius`. */
double niceRingStep(double radius, int target_rings);

}  // namespace core
