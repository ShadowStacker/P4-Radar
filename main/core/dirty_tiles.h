#pragma once

#include <cstdint>
#include <vector>

namespace core {

/** A rectangle in pixels, both corners included. */
struct Box {
  int32_t x1, y1, x2, y2;
};

/**
 * Which parts of the screen a frame has to redraw, on a coarse grid of
 * tiles: areas and sweep sectors mark tiles, and rects() turns them into a
 * few merged rectangles (redrawing a little extra is cheaper than a long
 * list of tiny areas, and LVGL keeps at most 32).
 */
class DirtyTiles {
 public:
  DirtyTiles(int width, int height, int tiles_per_side);

  void add(const Box& b);
  /** A line, `pad` px thick each side: only the tiles along it (a diagonal's bounding box would be far more). */
  void addSegment(float x1, float y1, float x2, float y2, float pad);
  /**
   * The wedge from (cx, cy) between `from_deg` and `from_deg + span_deg`
   * (clockwise from north, like a radar sweep), out to infinity, widened by
   * `pad` pixels. span_deg >= 360 marks everything.
   */
  void addSector(float cx, float cy, float from_deg, float span_deg, float pad);
  void addAll();
  bool empty() const;
  int markedTiles() const;
  /**
   * At most `max_rects` rectangles covering every marked tile: runs of
   * tiles per row, merged with the row below when they match, then, while
   * there are too many, the pair whose union adds the least area is merged.
   */
  std::vector<Box> rects(int max_rects) const;

 private:
  int w_, h_, n_;
  std::vector<uint8_t> tiles_;  // n_ x n_, row-major
  Box tileBox(int col, int row) const;
};

}  // namespace core
