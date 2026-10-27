#pragma once

#include "lvgl.h"

namespace ui {

/**
 * Draw buffer whose header and pixels live in one plat::allocLarge() block
 * (PSRAM on the device). lv_draw_buf_create() would use malloc(), which puts
 * small images in internal RAM; hundreds of sprites exhaust it.
 */
lv_draw_buf_t* createImageBuf(uint32_t w, uint32_t h, lv_color_format_t cf);
void destroyImageBuf(lv_draw_buf_t* buf);

}  // namespace ui
