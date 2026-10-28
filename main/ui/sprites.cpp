#include "ui/sprites.h"

#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#include "ui/image_buf.h"
#include "ui/lv_compat.h"
#include "ui/theme.h"

namespace ui::sprites {

namespace {

constexpr int kKinds = static_cast<int>(Kind::kCount);
lv_draw_buf_t* s_bufs[kKinds][kRotationSteps] = {};
int32_t s_size[kKinds] = {};

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

// Straight-wing light aircraft.
const V kProp[][3] = {
    {{0.00f, -0.95f}, {-0.13f, -0.70f}, {0.13f, -0.70f}},
    {{-0.13f, -0.70f}, {0.13f, -0.70f}, {0.07f, 0.85f}},
    {{-0.13f, -0.70f}, {0.07f, 0.85f}, {-0.07f, 0.85f}},
    {{-1.00f, -0.38f}, {1.00f, -0.38f}, {1.00f, -0.12f}},   // wing
    {{-1.00f, -0.38f}, {1.00f, -0.12f}, {-1.00f, -0.12f}},
    {{-0.42f, 0.66f}, {0.42f, 0.66f}, {0.42f, 0.86f}},      // tail
    {{-0.42f, 0.66f}, {0.42f, 0.86f}, {-0.42f, 0.86f}},
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

void drawRotor(lv_layer_t* layer, float cx, float cy, float scale, float heading_deg) {
  lv_draw_rect_dsc_t body;
  lv_draw_rect_dsc_init(&body);
  body.bg_color = lv_color_white();
  body.radius = LV_RADIUS_CIRCLE;
  const int32_t br = static_cast<int32_t>(scale * 0.38f);
  lv_area_t a = {static_cast<int32_t>(cx) - br, static_cast<int32_t>(cy) - br,
                 static_cast<int32_t>(cx) + br, static_cast<int32_t>(cy) + br};
  lv_draw_rect(layer, &body, &a);

  lv_draw_line_dsc_t ln;
  lv_draw_line_dsc_init(&ln);
  ln.color = lv_color_white();
  ln.width = sc(3) > 2 ? sc(3) : 2;
  ln.round_start = ln.round_end = 1;
  for (int k = 0; k < 2; ++k) {
    const float r = (heading_deg + 45.0f + 90.0f * k) * 3.14159265f / 180.0f;
    ln.p1.x = cx - std::sin(r) * scale;
    ln.p1.y = cy + std::cos(r) * scale;
    ln.p2.x = cx + std::sin(r) * scale;
    ln.p2.y = cy - std::cos(r) * scale;
    lv_draw_line(layer, &ln);
  }
  // Tail boom shows direction of travel.
  const float r = heading_deg * 3.14159265f / 180.0f;
  ln.p1.x = cx;
  ln.p1.y = cy;
  ln.p2.x = cx - std::sin(r) * scale * 0.95f;
  ln.p2.y = cy + std::cos(r) * scale * 0.95f;
  lv_draw_line(layer, &ln);
}

void drawGround(lv_layer_t* layer, float cx, float cy, float scale) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = lv_color_white();
  d.radius = static_cast<int32_t>(scale * 0.2f);
  const int32_t h = static_cast<int32_t>(scale * 0.45f);
  lv_area_t a = {static_cast<int32_t>(cx) - h, static_cast<int32_t>(cy) - h,
                 static_cast<int32_t>(cx) + h, static_cast<int32_t>(cy) + h};
  lv_draw_rect(layer, &d, &a);
}

}  // namespace

int32_t size(Kind kind) { return s_size[static_cast<int>(kind)]; }

void init(int scale_pct) {
  static int s_scale = 0;
  if (scale_pct < 50) scale_pct = 50;
  if (scale_pct > 200) scale_pct = 200;
  if (scale_pct == s_scale) return;
  s_scale = scale_pct;
  for (auto& kind : s_bufs) {
    for (lv_draw_buf_t*& b : kind) {
      if (b != nullptr) destroyImageBuf(b);
      b = nullptr;
    }
  }

  // Base sizes at 800 px; scaled for the 720 px panel and the user's choice.
  const int32_t base[kKinds] = {sc(30), sc(38), sc(48), sc(34), sc(16)};

  lv_obj_t* canvas = lv_canvas_create(lv_screen_active());
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);

  for (int k = 0; k < kKinds; ++k) {
    const int32_t sz = (base[k] * scale_pct / 100) | 1;  // odd size -> exact center pixel
    s_size[k] = sz;
    const float c = sz / 2.0f;
    const float scale = sz * 0.47f;
    for (int i = 0; i < kRotationSteps; ++i) {
      lv_draw_buf_t* buf = createImageBuf(sz, sz, LV_COLOR_FORMAT_ARGB8888);
      if (buf == nullptr) continue;
      lv_draw_buf_clear(buf, nullptr);
      lv_canvas_set_draw_buf(canvas, buf);
      lv_layer_t layer;
      lv_canvas_init_layer(canvas, &layer);
      const float hdg = i * (360.0f / kRotationSteps);
      switch (static_cast<Kind>(k)) {
        case Kind::Light: drawTris(&layer, kProp, sizeof(kProp) / sizeof(kProp[0]), c, c, scale, hdg); break;
        case Kind::Rotor: drawRotor(&layer, c, c, scale, hdg); break;
        case Kind::Ground: drawGround(&layer, c, c, scale); break;
        default: drawTris(&layer, kJet, sizeof(kJet) / sizeof(kJet[0]), c, c, scale, hdg); break;
      }
      lv_canvas_finish_layer(canvas, &layer);
      s_bufs[k][i] = buf;
    }
  }
  lv_obj_delete(canvas);

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

const lv_draw_buf_t* get(Kind kind, float heading_deg) {
  float h = std::fmod(heading_deg, 360.0f);
  if (h < 0) h += 360.0f;
  const int idx = static_cast<int>(std::lround(h / (360.0f / kRotationSteps))) % kRotationSteps;
  return s_bufs[static_cast<int>(kind)][idx];
}

Kind kindFor(const char* category, bool on_ground) {
  if (category != nullptr && category[0] == 'C') return Kind::Ground;  // surface vehicles
  if (category != nullptr && category[0] == 'A') {
    switch (category[1]) {
      case '1': return Kind::Light;
      case '2': return Kind::Medium;   // small jets
      case '3': return Kind::Medium;   // A320/B737 class
      case '4': return Kind::Heavy;    // B757
      case '5': return Kind::Heavy;    // widebodies
      case '7': return Kind::Rotor;
      default: break;
    }
  }
  if (category != nullptr && category[0] == 'B' && category[1] == '6') return Kind::Light;  // UAV
  (void)on_ground;
  return Kind::Medium;
}

}  // namespace ui::sprites
