#pragma once

#include "lvgl.h"

namespace ui {

enum class Page { Main, Wifi, Location, Display, Sound, Traffic, Favorites, About };

/** Builds the whole UI. Call with the LVGL lock held. */
void init(lv_display_t* disp);

/** Opens the settings screen at a page. */
void openSettings(Page page = Page::Main);
/** Returns from settings to the radar. */
void closeSettings();

/** Recreates every screen (after a theme change). */
void rebuild();

}  // namespace ui
