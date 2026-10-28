// Keeps the LCD backlight off from the first bootloader instructions.
//
// The backlight enable (GPIO26 on the Waveshare ESP32-P4 3.4"/4" boards) is
// active-low and nothing drives it until the app starts the display, so the
// backlight used to come on at reset and show the panel's uninitialized,
// cyan-ish picture for a second or two. The app takes the pin over (via the
// BSP's LEDC backlight) and turns it on once a real frame is on screen.

#include "esp_rom_gpio.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_struct.h"

#define BACKLIGHT_GPIO 26

/* Referenced by the bootloader build (-u) so this file is always linked. */
void bootloader_hooks_include(void) {}

void bootloader_before_init(void) {
  gpio_ll_set_level(&GPIO, BACKLIGHT_GPIO, 1);  // high = backlight off
  esp_rom_gpio_pad_select_gpio(BACKLIGHT_GPIO);
  esp_rom_gpio_connect_out_signal(BACKLIGHT_GPIO, SIG_GPIO_OUT_IDX, false, false);
  gpio_ll_output_enable(&GPIO, BACKLIGHT_GPIO);
}
