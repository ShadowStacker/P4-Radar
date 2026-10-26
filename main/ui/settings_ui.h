#pragma once

#include "lvgl.h"
#include "ui/ui.h"

namespace ui::settings_ui {

/** Creates the settings screen showing `page`. Returns the screen object. */
lv_obj_t* create(Page page);
/** Stops the page refresh timer (screen is being deleted). */
void destroy();

}  // namespace ui::settings_ui
