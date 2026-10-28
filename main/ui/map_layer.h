#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "app/maps.h"
#include "core/viewport.h"
#include "lvgl.h"

namespace ui {

/**
 * Background map for the radar. Keeps recently used decoded tiles, composes
 * them into one screen-sized picture when the view settles (and as tiles
 * arrive), and draws that picture under the radar: as is when the view
 * matches, shifted and scaled during pans and pinches.
 */
class MapLayer {
 public:
  void create(lv_obj_t* parent);
  void destroy();

  /**
   * Once per frame: takes finished tiles and asks for missing ones. Returns
   * true when the composed picture changed (the static layer must redraw).
   */
  bool update(const core::Viewport& vp, const app::maps::Look& look, bool busy, lv_color_t bg);

  /** Draws the picture for the current view; false if there is none yet. */
  bool draw(lv_layer_t* layer, const core::Viewport& vp) const;

  bool active() const { return look_.variant != core::tiles::Variant::None; }
  core::tiles::Variant variant() const { return look_.variant; }

 private:
  struct Tile {
    lv_draw_buf_t* buf;
    uint16_t* pixels;
    uint32_t used;  // frame counter when last needed
  };
  struct Needed {
    uint64_t key;
    float dist;  // from the screen center, for priority
  };

  void clearTiles();
  void computeNeeded(const core::Viewport& vp);
  void compose(const core::Viewport& vp, lv_color_t bg);
  /** Draws a tile (any zoom) at its place on screen, limited to `clip`. */
  void drawTile(lv_layer_t* layer, const core::Viewport& vp, uint64_t key, const lv_area_t& clip) const;
  lv_area_t tileRect(const core::Viewport& vp, uint64_t key) const;

  app::maps::Look look_;
  std::unordered_map<uint64_t, Tile> tiles_;
  std::vector<Needed> needed_;
  std::vector<uint64_t> last_request_;
  uint32_t frame_ = 0;

  lv_obj_t* canvas_ = nullptr;  // hidden, used to draw into pic_
  lv_draw_buf_t* pic_ = nullptr;
  bool pic_valid_ = false;
  bool pic_dirty_ = false;
  core::LatLon pic_center_;
  double pic_kmpp_ = 0;
  uint32_t last_compose_ms_ = 0;
};

}  // namespace ui
