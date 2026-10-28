#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/trace.h"
#include "core/traffic.h"
#include "core/viewport.h"
#include "lvgl.h"
#include "ui/map_layer.h"

namespace ui {

/**
 * The full-screen radar: a cached static layer (background, range rings,
 * compass bezel, runways, trails) plus a per-frame overlay (sweep beam,
 * aircraft, labels). Handles one-finger pan, two-finger pinch zoom, tap to
 * select, double-tap zoom and long-press.
 */
class RadarView {
 public:
  static RadarView& get();

  void create(lv_obj_t* parent);
  /** Releases the timer and static buffer (objects die with their screen). */
  void destroy();
  /** Stops rendering while another screen (settings) is shown. */
  void setPaused(bool paused);

  core::Viewport& viewport() { return vp_; }

  /** Animates back to the home location and home zoom. */
  void goHome(bool animate = true);
  void zoomBy(double factor);
  bool atHome() const;

  const core::Track* selected() const;
  void clearSelection();
  void setFollow(bool on);
  bool following() const { return follow_; }

  /** Crosshair mode used by "pick home on map". */
  void setPickMode(bool on);
  bool pickMode() const { return pick_mode_; }

  /** Forces the static layer to re-render (after settings/theme changes). */
  void markStaticDirty() { static_dirty_ = true; }

  size_t visibleCount() const { return blips_.size(); }
  /** Map shown under the radar (None when off). */
  core::tiles::Variant mapVariant() const { return map_.variant(); }

  std::function<void()> on_selection_changed;
  std::function<void(core::LatLon)> on_long_press;

 private:
  enum class Gesture { None, Undecided, Drag, Pinch, PinchDone };

  struct Blip {
    int track_idx;
    float x, y;
    float heading;
    lv_color_t color;
    lv_opa_t opa;
    uint8_t kind;
    bool has_vector;
    float vx, vy;  // speed vector end
    bool has_head;
    float hx, hy;  // last trail point (trail head joins it to the blip)
    bool selected;
    bool emergency;
    bool favorite;
    bool label;
    lv_area_t label_area;
    char line1[12];
    char line2[28];
  };

  static void onTimer(lv_timer_t* t);
  static void onEvent(lv_event_t* e);
  static void onDrawOverlay(lv_event_t* e);

  void frame();
  void handlePress(lv_event_t* e);
  void handlePressing(lv_event_t* e);
  void handleRelease(lv_event_t* e);
  void handleTap(lv_point_t p);

  void prepareBlips(uint32_t now);
  void placeLabels();
  void renderStatic(bool with_trails);
  void drawHistory(lv_layer_t* layer, bool fine);
  void syncTrace();
  void drawOverlay(lv_layer_t* layer);
  void drawSweep(lv_layer_t* layer);
  void drawClockHands(lv_layer_t* layer);
  void updateQueryArea(bool force_refresh);
  int findSelected() const;

  core::Viewport vp_;
  bool vp_initialized_ = false;
  lv_obj_t* canvas_ = nullptr;   // static layer
  lv_draw_buf_t* static_buf_ = nullptr;
  lv_obj_t* overlay_ = nullptr;  // per-frame drawing + touch target
  lv_timer_t* timer_ = nullptr;

  std::vector<core::Track> tracks_;
  uint32_t tracks_version_ = 0;
  std::vector<Blip> blips_;
  std::vector<core::Favorite> favorites_;  // refreshed each frame

  // Static layer invalidation
  bool static_dirty_ = true;
  bool static_has_trails_ = false;
  core::LatLon static_center_;
  double static_kmpp_ = 0;
  uint32_t static_tracks_version_ = 0;

  // Sweep
  float sweep_deg_ = 0;
  uint32_t last_frame_ms_ = 0;
  core::PointF sweep_center_;

  // Selection / follow
  std::string selected_hex_;
  MapLayer map_;

  // Downloaded flight history of the selected aircraft
  std::string trace_req_hex_;
  core::trace::Trace trace_;
  uint32_t trace_version_ = 0;
  bool follow_ = false;
  bool pick_mode_ = false;

  // Touch
  Gesture gesture_ = Gesture::None;
  lv_point_t press_pt_ = {0, 0};
  uint32_t press_ms_ = 0;
  uint32_t last_tap_ms_ = 0;
  lv_point_t last_tap_pt_ = {0, 0};
  bool interacting_ = false;

  // Query area last sent to the poller
  core::LatLon query_center_;
  double query_radius_km_ = 0;
};

}  // namespace ui
