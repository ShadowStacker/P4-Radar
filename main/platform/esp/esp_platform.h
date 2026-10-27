#pragma once

#include "lvgl.h"

/** Device bring-up steps, called in order from app_main(). */
namespace plat::esp {

/** Panel, backlight, LVGL adapter task and multi-touch input. */
lv_display_t* displayStart();
/** ES8311 codec + speaker amp; pre-renders the sound effects. */
void audioStart();
/** Netif, event loop and Wi-Fi (ESP32-C6 via ESP-Hosted). False if the radio is unavailable. */
bool wifiStart();

/** Plain status screen shown while the rest of the system starts. */
void bootScreen(const char* status);

bool lvglLock();
void lvglUnlock();

}  // namespace plat::esp
