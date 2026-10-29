#pragma once

#include "lvgl.h"

/** Device bring-up steps, called in order from app_main(). */
namespace plat::esp {

/**
 * Counts this start (NVS must be up). After the third start in a row that
 * didn't last 20 s, switches to the other app slot and restarts (doesn't
 * return), if that slot holds a working version.
 */
void bootCheck();

/**
 * Panel, backlight, LVGL adapter task and multi-touch input. Touch is mapped
 * for a UI rotated by `quarter_turns` x 90 deg clockwise; the picture itself
 * is rotated by ui::installDisplayRotation(). Fixed until the next boot.
 */
lv_display_t* displayStart(int quarter_turns);
/** ES8311 codec + speaker amp; pre-renders the sound effects. */
void audioStart();
/** Netif, event loop and Wi-Fi (ESP32-C6 via ESP-Hosted). False if the radio is unavailable. */
bool wifiStart();

/** Plain status screen shown while the rest of the system starts. */
void bootScreen(const char* status);

bool lvglLock();
void lvglUnlock();

}  // namespace plat::esp
