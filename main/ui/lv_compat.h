#pragma once

// Small shims over LVGL 9.x API differences (tested against 9.2 and 9.6).

#include "lvgl.h"
// lv_image_cache_drop(): moved between headers in 9.x.
#if __has_include("src/misc/cache/instance/lv_image_cache.h")
#include "src/misc/cache/instance/lv_image_cache.h"
#elif __has_include("src/misc/cache/lv_image_cache.h")
#include "src/misc/cache/lv_image_cache.h"
#endif

namespace ui {

inline void triangleDscSetColor(lv_draw_triangle_dsc_t* d, lv_color_t c, lv_opa_t opa) {
#if LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR < 3
  d->bg_color = c;
  d->bg_opa = opa;
#else
  d->color = c;
  d->opa = opa;
#endif
}

/** Absolute clip area of the layer being drawn (the current render stripe). */
inline const lv_area_t& layerClip(const lv_layer_t* layer) { return layer->_clip_area; }

}  // namespace ui
