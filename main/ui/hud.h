#pragma once

#include "lvgl.h"

namespace ui::hud {

/** Buttons, status pill and aircraft info card drawn over the radar. */
void create(lv_obj_t* parent);
void destroy();

/** Shows the crosshair + "Set home here" controls. */
void enterPickMode();
void exitPickMode();

/** Hides (or shows) the buttons, for a screenshot; returns whether they were hidden before. */
bool hideForSnapshot(bool hide);

}  // namespace ui::hud
