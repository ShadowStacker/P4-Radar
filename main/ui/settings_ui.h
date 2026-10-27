#pragma once

#include "lvgl.h"
#include "ui/ui.h"

namespace ui::settings_ui {

/** Creates the settings screen showing `page`. Returns the screen object. */
lv_obj_t* create(Page page, int32_t scroll_y = 0);
/** Page showing and how far it is scrolled, to restore them after a rebuild. */
Page currentPage();
int32_t scrollY();
/** Stops the page refresh timer (screen is being deleted). */
void destroy();

}  // namespace ui::settings_ui
