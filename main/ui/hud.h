#pragma once

#include "lvgl.h"

namespace ui::hud {

/** Buttons, status pill and aircraft info card drawn over the radar. */
void create(lv_obj_t* parent);
void destroy();

/** Shows the crosshair + "Set home here" controls. */
void enterPickMode();
void exitPickMode();

}  // namespace ui::hud
