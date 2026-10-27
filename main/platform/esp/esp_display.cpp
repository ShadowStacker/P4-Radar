// Display + touch bring-up. Mirrors bsp_display_start_with_config() from the
// Waveshare BSP, except that touch is read by our own LVGL input device so the
// UI can see every finger (the BSP's adapter only reports one point, which
// rules out pinch-zoom).

#include <atomic>
#include <cstring>

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "core/rotation.h"
#include "platform/esp/esp_platform.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "display";

constexpr int kMaxPoints = 5;

esp_lcd_touch_handle_t s_touch = nullptr;
std::atomic<bool> s_backlight_ready{false};

// Written and read only from the LVGL task (indev read + UI event handlers).
TouchPoint s_points[kMaxPoints];
int s_point_count = 0;
int s_rotation = 0;  // quarter turns clockwise

void touchRead(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;
  esp_lcd_touch_point_data_t pts[kMaxPoints];
  uint8_t count = 0;
  if (esp_lcd_touch_read_data(s_touch) == ESP_OK) {
    esp_lcd_touch_get_data(s_touch, pts, &count, kMaxPoints);
  }
  s_point_count = count;
  for (int i = 0; i < count; ++i) {
    int x, y;
    core::panelToUi(pts[i].x, pts[i].y, s_rotation, BSP_LCD_H_RES, BSP_LCD_V_RES, &x, &y);
    s_points[i] = {static_cast<int16_t>(x), static_cast<int16_t>(y)};
  }
  if (count > 0) {
    data->point.x = s_points[0].x;
    data->point.y = s_points[0].y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;  // LVGL keeps the last point
  }
}

}  // namespace

int touchPoints(TouchPoint* out, int max) {
  const int n = s_point_count < max ? s_point_count : max;
  std::memcpy(out, s_points, n * sizeof(TouchPoint));
  return n;
}

void setBrightness(int percent) {
  if (!s_backlight_ready) return;
  bsp_display_brightness_set(percent);
}

namespace esp {

lv_display_t* displayStart(int quarter_turns) {
  s_rotation = quarter_turns & 3;
  esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
  adapter_cfg.task_stack_size = 24 * 1024;  // canvas rendering + label layout
  adapter_cfg.task_priority = 5;
  adapter_cfg.task_core_id = 1;  // keep Wi-Fi/TLS on core 0
  adapter_cfg.task_max_delay_ms = 10;
  ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_cfg));

  esp_lcd_panel_handle_t panel = nullptr;
  esp_lcd_panel_io_handle_t io = nullptr;
  ESP_ERROR_CHECK(bsp_display_new(nullptr, &panel, &io));  // also sets up the backlight PWM
  bsp_display_brightness_set(0);  // stay dark until the first frame is ready
  s_backlight_ready = true;

  esp_lv_adapter_display_config_t disp_cfg;
  std::memset(&disp_cfg, 0, sizeof(disp_cfg));
  disp_cfg.panel = panel;
  disp_cfg.panel_io = io;
  disp_cfg.profile.interface = ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI;
  // Always native orientation: rotation is done before the flush by
  // ui::installDisplayRotation(), not by the adapter (see display_rotation.cpp).
  disp_cfg.profile.rotation = ESP_LV_ADAPTER_ROTATE_0;
  disp_cfg.profile.hor_res = BSP_LCD_H_RES;
  disp_cfg.profile.ver_res = BSP_LCD_V_RES;
  disp_cfg.profile.buffer_height = 100;  // 100-line stripes in internal RAM
  disp_cfg.profile.use_psram = false;
  disp_cfg.profile.enable_ppa_accel = false;
  disp_cfg.profile.require_double_buffer = false;
  disp_cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
  lv_display_t* disp = esp_lv_adapter_register_display(&disp_cfg);
  if (disp == nullptr) {
    ESP_LOGE(TAG, "display registration failed");
    abort();
  }

  bsp_display_cfg_t touch_cfg;
  std::memset(&touch_cfg, 0, sizeof(touch_cfg));
  if (bsp_touch_new(&touch_cfg, &s_touch) == ESP_OK && s_touch != nullptr) {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchRead);
    lv_indev_set_display(indev, disp);
  } else {
    ESP_LOGE(TAG, "touch controller not found - UI will be view-only");
  }

  ESP_ERROR_CHECK(esp_lv_adapter_start());
  ESP_LOGI(TAG, "display %dx%d ready, rotated %d deg", BSP_LCD_H_RES, BSP_LCD_V_RES, s_rotation * 90);
  return disp;
}

void bootScreen(const char* status) {
  static lv_obj_t* s_label = nullptr;
  if (!lvglLock()) return;
  if (s_label == nullptr) {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x020D06), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "PLANE RADAR");
    lv_obj_set_style_text_color(title, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_40, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -30);
    s_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label, lv_color_hex(0x8EA3AE), 0);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_22, 0);
    lv_obj_align(s_label, LV_ALIGN_CENTER, 0, 30);
  }
  lv_label_set_text(s_label, status);
  lvglUnlock();
  // Light the panel as soon as there is something to show; a dark screen
  // gives no clue where boot stopped.
  if (bsp_display_brightness_get() == 0) bsp_display_brightness_set(60);
}

bool lvglLock() { return esp_lv_adapter_lock(-1) == ESP_OK; }
void lvglUnlock() { esp_lv_adapter_unlock(); }

}  // namespace esp

}  // namespace plat
