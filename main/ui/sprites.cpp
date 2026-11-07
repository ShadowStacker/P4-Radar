#include "ui/sprites.h"

#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "core/inflate.h"
#include "core/sprite_raster.h"
#include "data/aircraft_icons.h"
#include "ui/image_buf.h"
#include "ui/lv_compat.h"
#include "ui/theme.h"

namespace ui::sprites {

namespace {

constexpr int kKinds = static_cast<int>(Kind::kCount);
// [0] full size, [1] small (used when zoomed out with many aircraft).
lv_draw_buf_t* s_bufs[2][kKinds][kRotationSteps] = {};
int32_t s_size[2][kKinds] = {};
constexpr int kSmallPct = 62;
std::string s_masks[kKinds];  // alpha, s_mask_size[k] squared; nose up
int s_mask_size[kKinds] = {};

struct V {
  float x, y;
};

// Airliner silhouette, nose toward -y, spanning roughly [-1, 1].
const V kJet[][3] = {
    {{0.00f, -1.00f}, {-0.11f, -0.74f}, {0.11f, -0.74f}},   // nose
    {{-0.11f, -0.74f}, {0.11f, -0.74f}, {0.11f, 0.86f}},    // fuselage
    {{-0.11f, -0.74f}, {0.11f, 0.86f}, {-0.11f, 0.86f}},
    {{0.00f, -0.30f}, {-0.98f, 0.18f}, {-0.98f, 0.32f}},    // left wing
    {{0.00f, -0.30f}, {-0.98f, 0.32f}, {0.00f, 0.12f}},
    {{0.00f, -0.30f}, {0.98f, 0.18f}, {0.98f, 0.32f}},      // right wing
    {{0.00f, -0.30f}, {0.98f, 0.32f}, {0.00f, 0.12f}},
    {{0.00f, 0.58f}, {-0.40f, 0.88f}, {-0.40f, 0.99f}},     // left stabilizer
    {{0.00f, 0.58f}, {-0.40f, 0.99f}, {0.00f, 0.92f}},
    {{0.00f, 0.58f}, {0.40f, 0.88f}, {0.40f, 0.99f}},       // right stabilizer
    {{0.00f, 0.58f}, {0.40f, 0.99f}, {0.00f, 0.92f}},
};

void drawTris(lv_layer_t* layer, const V (*tris)[3], int count, float cx, float cy, float scale,
              float heading_deg) {
  const float r = heading_deg * 3.14159265f / 180.0f;
  const float c = std::cos(r), s = std::sin(r);
  lv_draw_triangle_dsc_t d;
  lv_draw_triangle_dsc_init(&d);
  triangleDscSetColor(&d, lv_color_white(), LV_OPA_COVER);
  for (int i = 0; i < count; ++i) {
    for (int k = 0; k < 3; ++k) {
      const float x = tris[i][k].x * scale, y = tris[i][k].y * scale;
      d.p[k].x = static_cast<lv_value_precise_t>(cx + x * c - y * s);
      d.p[k].y = static_cast<lv_value_precise_t>(cy + x * s + y * c);
    }
    lv_draw_triangle(layer, &d);
  }
}

}  // namespace

int32_t size(Kind kind, bool small) { return s_size[small ? 1 : 0][static_cast<int>(kind)]; }

void init(int scale_pct) {
  static int s_scale = 0;
  if (scale_pct < 50) scale_pct = 50;
  if (scale_pct > 200) scale_pct = 200;
  if (scale_pct == s_scale) return;
  s_scale = scale_pct;
  for (auto& set : s_bufs) {
    for (auto& kind : set) {
      for (lv_draw_buf_t*& b : kind) {
        if (b != nullptr) destroyImageBuf(b);
        b = nullptr;
      }
    }
  }
  // The icon masks, unpacked once (PSRAM). Sprites are made from them on
  // first use, one heading at a time (see get()).
  if (s_masks[0].empty()) {
    for (int k = 0; k < kKinds; ++k) {
      const char* name = core::aircraft::iconName(static_cast<Kind>(k));
      for (int m = 0; m < data::kIconMaskCount; ++m) {
        const data::IconMask& im = data::kIconMasks[m];
        if (std::strcmp(im.name, name) != 0) continue;
        std::string px;
        if (core::inflateRaw(im.data, im.len, &px, static_cast<size_t>(im.size) * im.size) &&
            px.size() == static_cast<size_t>(im.size) * im.size) {
          s_masks[k] = std::move(px);
          s_mask_size[k] = im.size;
        }
        break;
      }
    }
  }
  // Sizes at 800 px (light aircraft small, jumbos big); scaled for the
  // 720 px panel and the user's choice. Odd: an exact center pixel.
  for (int set = 0; set < 2; ++set) {
    const int pct = set == 0 ? scale_pct : scale_pct * kSmallPct / 100;
    for (int k = 0; k < kKinds; ++k) {
      s_size[set][k] = (sc(core::aircraft::baseSize(static_cast<Kind>(k))) * pct / 100) | 1;
    }
  }

  // Stars used by the radar badge and the favorite buttons. Pre-rendered here:
  // rendering on first use could happen inside a draw pass.
  star(starBadgeSize());
  star(starButtonSize());
}

int32_t starBadgeSize() { return sc(18) | 1; }
int32_t starButtonSize() { return sc(30) | 1; }

const lv_draw_buf_t* star(int32_t size) {
  static std::vector<std::pair<int32_t, lv_draw_buf_t*>> s_cache;
  for (const auto& e : s_cache) {
    if (e.first == size) return e.second;
  }
  lv_draw_buf_t* buf = createImageBuf(size, size, LV_COLOR_FORMAT_ARGB8888);
  if (buf == nullptr) return nullptr;
  lv_draw_buf_clear(buf, nullptr);

  lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  lv_canvas_set_draw_buf(canvas, buf);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  // Fan of 10 triangles from the center: the star is star-shaped around it.
  const float c = size / 2.0f, ro = size * 0.49f, ri = ro * 0.42f;
  lv_draw_triangle_dsc_t d;
  lv_draw_triangle_dsc_init(&d);
  triangleDscSetColor(&d, lv_color_white(), LV_OPA_COVER);
  for (int i = 0; i < 10; ++i) {
    const float a0 = (i * 36.0f - 90.0f) * 3.14159265f / 180.0f;
    const float a1 = ((i + 1) * 36.0f - 90.0f) * 3.14159265f / 180.0f;
    const float r0 = (i % 2 == 0) ? ro : ri, r1 = (i % 2 == 0) ? ri : ro;
    d.p[0].x = c;
    d.p[0].y = c + size * 0.03f;  // optical center sits slightly low
    d.p[1].x = c + std::cos(a0) * r0;
    d.p[1].y = c + size * 0.03f + std::sin(a0) * r0;
    d.p[2].x = c + std::cos(a1) * r1;
    d.p[2].y = c + size * 0.03f + std::sin(a1) * r1;
    lv_draw_triangle(&layer, &d);
  }
  lv_canvas_finish_layer(canvas, &layer);
  lv_obj_delete(canvas);
  s_cache.push_back({size, buf});
  return buf;
}

const lv_draw_buf_t* clockIcon(int32_t size) {
  static std::vector<std::pair<int32_t, lv_draw_buf_t*>> s_cache;
  for (const auto& e : s_cache) {
    if (e.first == size) return e.second;
  }
  lv_draw_buf_t* buf = createImageBuf(size, size, LV_COLOR_FORMAT_ARGB8888);
  if (buf == nullptr) return nullptr;
  lv_draw_buf_clear(buf, nullptr);

  lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  lv_canvas_set_draw_buf(canvas, buf);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  // Round face with hands at ten past ten.
  const int32_t w = size / 9 > 2 ? size / 9 : 2;
  const float c = size / 2.0f;
  lv_draw_arc_dsc_t arc;
  lv_draw_arc_dsc_init(&arc);
  arc.color = lv_color_white();
  arc.width = w;
  arc.center.x = static_cast<int32_t>(c);
  arc.center.y = static_cast<int32_t>(c);
  arc.radius = static_cast<uint16_t>(c - 1);
  arc.start_angle = 0;
  arc.end_angle = 360;
  lv_draw_arc(&layer, &arc);
  lv_draw_line_dsc_t ln;
  lv_draw_line_dsc_init(&ln);
  ln.color = lv_color_white();
  ln.width = w;
  ln.round_start = ln.round_end = 1;
  ln.p1.x = c;
  ln.p1.y = c;
  ln.p2.x = c;
  ln.p2.y = c - size * 0.30f;  // minute hand to 12
  lv_draw_line(&layer, &ln);
  ln.p2.x = c + size * 0.20f;
  ln.p2.y = c + size * 0.06f;  // hour hand toward 3-4
  lv_draw_line(&layer, &ln);
  lv_canvas_finish_layer(canvas, &layer);
  lv_obj_delete(canvas);
  s_cache.push_back({size, buf});
  return buf;
}

const lv_draw_buf_t* planeIcon(int32_t size) {
  static std::vector<std::pair<int32_t, lv_draw_buf_t*>> s_cache;
  for (const auto& e : s_cache) {
    if (e.first == size) return e.second;
  }
  lv_draw_buf_t* buf = createImageBuf(size, size, LV_COLOR_FORMAT_ARGB8888);
  if (buf == nullptr) return nullptr;
  lv_draw_buf_clear(buf, nullptr);
  lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  lv_canvas_set_draw_buf(canvas, buf);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  const float c = size / 2.0f;
  drawTris(&layer, kJet, sizeof(kJet) / sizeof(kJet[0]), c, c, size * 0.48f, 0);
  lv_canvas_finish_layer(canvas, &layer);
  lv_obj_delete(canvas);
  s_cache.emplace_back(size, buf);
  return buf;
}

const lv_draw_buf_t* bookmarkIcon(int32_t size) {
  static std::vector<std::pair<int32_t, lv_draw_buf_t*>> s_cache;
  for (const auto& e : s_cache) {
    if (e.first == size) return e.second;
  }
  lv_draw_buf_t* buf = createImageBuf(size, size, LV_COLOR_FORMAT_ARGB8888);
  if (buf == nullptr) return nullptr;
  lv_draw_buf_clear(buf, nullptr);
  lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  lv_canvas_set_draw_buf(canvas, buf);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  // The ribbon: a solid body down to the notch, then two tails. The tails
  // overlap the body a little, so no seam shows where they meet.
  const float c = size / 2.0f, k = size * 0.5f;
  lv_draw_rect_dsc_t body;
  lv_draw_rect_dsc_init(&body);
  body.bg_color = lv_color_white();
  body.radius = size / 16;
  const lv_area_t box = {static_cast<int32_t>(c - 0.46f * k), static_cast<int32_t>(c - 0.82f * k),
                         static_cast<int32_t>(c + 0.46f * k) - 1, static_cast<int32_t>(c + 0.44f * k)};
  lv_draw_rect(&layer, &body, &box);
  static const V kTails[][3] = {
      {{-0.46f, 0.36f}, {0.04f, 0.36f}, {-0.46f, 0.82f}},
      {{-0.04f, 0.36f}, {0.46f, 0.36f}, {0.46f, 0.82f}},
  };
  drawTris(&layer, kTails, 2, c, c, k, 0);
  lv_canvas_finish_layer(canvas, &layer);
  lv_obj_delete(canvas);
  s_cache.emplace_back(size, buf);
  return buf;
}

const lv_draw_buf_t* searchIcon(int32_t size) {
  static std::vector<std::pair<int32_t, lv_draw_buf_t*>> s_cache;
  for (const auto& e : s_cache) {
    if (e.first == size) return e.second;
  }
  lv_draw_buf_t* buf = createImageBuf(size, size, LV_COLOR_FORMAT_ARGB8888);
  if (buf == nullptr) return nullptr;
  lv_draw_buf_clear(buf, nullptr);
  lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  lv_canvas_set_draw_buf(canvas, buf);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  const int32_t w = size / 9 > 2 ? size / 9 : 2;
  // Lens up and to the left, handle down to the lower right.
  lv_draw_arc_dsc_t arc;
  lv_draw_arc_dsc_init(&arc);
  arc.color = lv_color_white();
  arc.width = w;
  arc.center.x = size * 42 / 100;
  arc.center.y = size * 42 / 100;
  arc.radius = static_cast<uint16_t>(size * 30 / 100);
  arc.start_angle = 0;
  arc.end_angle = 360;
  lv_draw_arc(&layer, &arc);
  lv_draw_line_dsc_t ln;
  lv_draw_line_dsc_init(&ln);
  ln.color = lv_color_white();
  ln.width = w + 1;
  ln.round_start = ln.round_end = 1;
  ln.p1.x = size * 64 / 100;
  ln.p1.y = size * 64 / 100;
  ln.p2.x = size * 90 / 100;
  ln.p2.y = size * 90 / 100;
  lv_draw_line(&layer, &ln);
  lv_canvas_finish_layer(canvas, &layer);
  lv_obj_delete(canvas);
  s_cache.emplace_back(size, buf);
  return buf;
}

const lv_draw_buf_t* cameraIcon(int32_t size) {
  static std::vector<std::pair<int32_t, lv_draw_buf_t*>> s_cache;
  for (const auto& e : s_cache) {
    if (e.first == size) return e.second;
  }
  lv_draw_buf_t* buf = createImageBuf(size, size, LV_COLOR_FORMAT_ARGB8888);
  if (buf == nullptr) return nullptr;
  lv_draw_buf_clear(buf, nullptr);

  lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  lv_canvas_set_draw_buf(canvas, buf);
  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  const int32_t w = size / 10 > 2 ? size / 10 : 2;
  // Body: an outlined rounded box, with the viewfinder bump on top.
  lv_draw_rect_dsc_t body;
  lv_draw_rect_dsc_init(&body);
  body.bg_opa = LV_OPA_TRANSP;
  body.border_color = lv_color_white();
  body.border_width = w;
  body.radius = size / 7;
  const lv_area_t box = {1, size * 27 / 100, size - 2, size * 86 / 100};
  lv_draw_rect(&layer, &body, &box);
  lv_draw_rect_dsc_t bump;
  lv_draw_rect_dsc_init(&bump);
  bump.bg_color = lv_color_white();
  bump.radius = w / 2;
  const lv_area_t top = {size * 32 / 100, size * 14 / 100, size * 66 / 100, size * 27 / 100};
  lv_draw_rect(&layer, &bump, &top);
  // Lens.
  lv_draw_arc_dsc_t arc;
  lv_draw_arc_dsc_init(&arc);
  arc.color = lv_color_white();
  arc.width = w;
  arc.center.x = size / 2;
  arc.center.y = size * 57 / 100;
  arc.radius = static_cast<uint16_t>(size * 19 / 100);
  arc.start_angle = 0;
  arc.end_angle = 360;
  lv_draw_arc(&layer, &arc);
  lv_canvas_finish_layer(canvas, &layer);
  lv_obj_delete(canvas);
  s_cache.push_back({size, buf});
  return buf;
}

const lv_draw_buf_t* get(Kind kind, float heading_deg, bool small) {
  const int k = static_cast<int>(kind);
  if (k < 0 || k >= kKinds) return nullptr;
  float h = std::fmod(heading_deg, 360.0f);
  if (h < 0) h += 360.0f;
  const int idx = static_cast<int>(std::lround(h / (360.0f / kRotationSteps))) % kRotationSteps;
  const int set = small ? 1 : 0;
  lv_draw_buf_t*& buf = s_bufs[set][k][idx];
  if (buf == nullptr && !s_masks[k].empty()) {
    // First time this icon is needed at this heading: rotate the mask into an
    // alpha-only sprite (drawn in the aircraft's color). Plain CPU work, so it
    // is fine even in the middle of drawing.
    const int32_t sz = s_size[set][k];
    buf = createImageBuf(sz, sz, LV_COLOR_FORMAT_A8);
    if (buf != nullptr) {
      core::sprite::rotateMask(reinterpret_cast<const uint8_t*>(s_masks[k].data()), s_mask_size[k], buf->data, sz,
                               static_cast<int>(buf->header.stride), idx * (360.0f / kRotationSteps));
    }
  }
  return buf;
}

Kind kindFor(const char* type, const char* category) { return core::aircraft::iconFor(type, category); }

}  // namespace ui::sprites
