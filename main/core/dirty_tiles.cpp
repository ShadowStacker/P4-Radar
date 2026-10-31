#include "core/dirty_tiles.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace core {

namespace {

/** Angle of (x, y) seen from (cx, cy), clockwise from north, in [0, 360). */
float bearing(float cx, float cy, float x, float y) {
  float a = std::atan2(x - cx, -(y - cy)) * 57.2957795f;
  return a < 0 ? a + 360.0f : a;
}

float wrap360(float a) {
  a = std::fmod(a, 360.0f);
  return a < 0 ? a + 360.0f : a;
}

/** A run of marked tiles in one row, [c0, c1]. */
struct Run {
  int c0, c1;
  bool operator==(const Run& o) const { return c0 == o.c0 && c1 == o.c1; }
};

}  // namespace

DirtyTiles::DirtyTiles(int width, int height, int tiles_per_side)
    : w_(width), h_(height), n_(std::max(1, tiles_per_side)), tiles_(static_cast<size_t>(n_) * n_, 0) {}

Box DirtyTiles::tileBox(int col, int row) const {
  return {static_cast<int32_t>(col * w_ / n_), static_cast<int32_t>(row * h_ / n_),
          static_cast<int32_t>((col + 1) * w_ / n_ - 1), static_cast<int32_t>((row + 1) * h_ / n_ - 1)};
}

void DirtyTiles::add(const Box& b) {
  if (b.x2 < 0 || b.y2 < 0 || b.x1 >= w_ || b.y1 >= h_ || b.x2 < b.x1 || b.y2 < b.y1) return;
  // (int32_t isn't int on every target: compare as int.)
  const int c0 = std::max(0, static_cast<int>(b.x1)) * n_ / w_, c1 = std::min(w_ - 1, static_cast<int>(b.x2)) * n_ / w_;
  const int r0 = std::max(0, static_cast<int>(b.y1)) * n_ / h_, r1 = std::min(h_ - 1, static_cast<int>(b.y2)) * n_ / h_;
  for (int r = r0; r <= r1; ++r) {
    for (int c = c0; c <= c1; ++c) tiles_[static_cast<size_t>(r) * n_ + c] = 1;
  }
}

void DirtyTiles::addSegment(float x1, float y1, float x2, float y2, float pad) {
  // Steps of under half a tile, each marking a small box around the point.
  const float tile = static_cast<float>(std::min(w_, h_)) / n_;
  const float len = std::hypot(x2 - x1, y2 - y1);
  const int steps = std::max(1, static_cast<int>(std::ceil(len / (tile * 0.45f))));
  for (int i = 0; i <= steps; ++i) {
    const float t = static_cast<float>(i) / steps;
    const float x = x1 + (x2 - x1) * t, y = y1 + (y2 - y1) * t;
    add({static_cast<int32_t>(x - pad), static_cast<int32_t>(y - pad), static_cast<int32_t>(x + pad),
         static_cast<int32_t>(y + pad)});
  }
}

void DirtyTiles::addAll() { std::fill(tiles_.begin(), tiles_.end(), 1); }

void DirtyTiles::addSector(float cx, float cy, float from_deg, float span_deg, float pad) {
  if (span_deg >= 360.0f) {
    addAll();
    return;
  }
  from_deg = wrap360(from_deg);
  for (int r = 0; r < n_; ++r) {
    for (int c = 0; c < n_; ++c) {
      const Box t = tileBox(c, r);
      const float x1 = t.x1 - pad, y1 = t.y1 - pad, x2 = t.x2 + 1 + pad, y2 = t.y2 + 1 + pad;
      bool hit;
      if (cx >= x1 && cx <= x2 && cy >= y1 && cy <= y2) {
        hit = true;  // the sweep's center is in this tile
      } else {
        // The tile's angular extent as seen from the center (under 180 deg,
        // since the center is outside it), against the sector.
        const float corners[4] = {bearing(cx, cy, x1, y1), bearing(cx, cy, x2, y1), bearing(cx, cy, x1, y2),
                                  bearing(cx, cy, x2, y2)};
        float lo = 0, hi = 0;
        for (float a : corners) {
          float rel = wrap360(a - corners[0]);
          if (rel > 180.0f) rel -= 360.0f;
          lo = std::min(lo, rel);
          hi = std::max(hi, rel);
        }
        const float start = wrap360(corners[0] + lo - from_deg);  // tile start, relative to the sector
        hit = start <= span_deg || start + (hi - lo) >= 360.0f;
      }
      if (hit) tiles_[static_cast<size_t>(r) * n_ + c] = 1;
    }
  }
}

bool DirtyTiles::empty() const { return markedTiles() == 0; }

int DirtyTiles::markedTiles() const {
  return static_cast<int>(std::count(tiles_.begin(), tiles_.end(), 1));
}

std::vector<Box> DirtyTiles::rects(int max_rects) const {
  // Runs of tiles per row, merged down into the next row when they match.
  std::vector<Box> out;
  std::vector<Run> above;
  std::vector<size_t> above_rect;
  for (int r = 0; r < n_; ++r) {
    std::vector<Run> runs;
    for (int c = 0; c < n_; ++c) {
      if (!tiles_[static_cast<size_t>(r) * n_ + c]) continue;
      if (!runs.empty() && runs.back().c1 == c - 1) runs.back().c1 = c;
      else runs.push_back({c, c});
    }
    std::vector<size_t> rect_of(runs.size());
    for (size_t i = 0; i < runs.size(); ++i) {
      const Box first = tileBox(runs[i].c0, r), last = tileBox(runs[i].c1, r);
      size_t j = 0;
      while (j < above.size() && !(above[j] == runs[i])) ++j;
      if (j < above.size()) {
        out[above_rect[j]].y2 = last.y2;
        rect_of[i] = above_rect[j];
      } else {
        out.push_back({first.x1, first.y1, last.x2, last.y2});
        rect_of[i] = out.size() - 1;
      }
    }
    above = runs;
    above_rect = rect_of;
  }
  if (max_rects < 1) max_rects = 1;
  if (static_cast<int>(out.size()) <= max_rects) return out;

  // Too many: merge the pair that adds the least extra area, again and again
  // (each rectangle remembers its best partner, so a merge costs one pass).
  auto area = [](const Box& b) { return static_cast<int64_t>(b.x2 - b.x1 + 1) * (b.y2 - b.y1 + 1); };
  auto join = [](const Box& a, const Box& b) {
    return Box{std::min(a.x1, b.x1), std::min(a.y1, b.y1), std::max(a.x2, b.x2), std::max(a.y2, b.y2)};
  };
  auto cost = [&](size_t i, size_t j) { return area(join(out[i], out[j])) - area(out[i]) - area(out[j]); };
  std::vector<bool> alive(out.size(), true);
  std::vector<size_t> best(out.size(), 0);
  std::vector<int64_t> best_cost(out.size(), INT64_MAX);
  auto findBest = [&](size_t i) {
    best_cost[i] = INT64_MAX;
    for (size_t j = 0; j < out.size(); ++j) {
      if (j == i || !alive[j]) continue;
      const int64_t c = cost(i, j);
      if (c < best_cost[i]) {
        best_cost[i] = c;
        best[i] = j;
      }
    }
  };
  for (size_t i = 0; i < out.size(); ++i) findBest(i);
  int count = static_cast<int>(out.size());
  while (count > max_rects) {
    size_t a = SIZE_MAX;
    for (size_t i = 0; i < out.size(); ++i) {
      if (alive[i] && (a == SIZE_MAX || best_cost[i] < best_cost[a])) a = i;
    }
    const size_t b = best[a];
    out[a] = join(out[a], out[b]);
    alive[b] = false;
    --count;
    // Partners that pointed at either of the two look again; the rest only
    // compare against the merged rectangle.
    for (size_t i = 0; i < out.size(); ++i) {
      if (!alive[i] || i == a) continue;
      if (best[i] == a || best[i] == b) {
        findBest(i);
      } else {
        const int64_t c = cost(i, a);
        if (c < best_cost[i]) {
          best_cost[i] = c;
          best[i] = a;
        }
      }
    }
    findBest(a);
  }
  std::vector<Box> merged;
  for (size_t i = 0; i < out.size(); ++i) {
    if (alive[i]) merged.push_back(out[i]);
  }
  return merged;
}

}  // namespace core
