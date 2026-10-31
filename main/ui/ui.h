#pragma once

#include "lvgl.h"

namespace ui {

enum class Page {
  Main, Wifi, Location, Display, Sound, Traffic, Filters, Favorites, DayLog, Updates, About,
  CustomTheme, Clock, TimeZone, AlarmEdit, SdCard, Phone, Watchlist, Silenced,
};

/** Builds the whole UI. Call with the LVGL lock held. */
void init(lv_display_t* disp);

/** Opens the settings screen at a page. */
void openSettings(Page page = Page::Main);
/** True while the settings screen is shown. */
bool settingsOpen();

/**
 * Returns from settings to the radar. With `discard`, every setting and
 * favorite goes back to how it was when settings were opened (Wi-Fi, which
 * is changed by connecting, stays as it is).
 */
void closeSettings(bool discard = false);

/**
 * Recreates every screen (after a theme change). If settings are open they
 * reopen at the page that was showing, scrolled to the same place.
 */
void rebuild();

/**
 * Saves what's on the screen (without the radar's buttons) as a PNG on the
 * SD card. False (with a message) without a card.
 */
bool takeScreenshot();

/** Asks "Install vX.Y.Z?" and starts the firmware update if confirmed (needs an available update). */
void confirmInstallUpdate();

}  // namespace ui
