// microSD card on SDMMC slot 0 (FAT), set up like the Waveshare BSP's
// bsp_sdcard_mount(), except that the card's power regulator (on-chip LDO 4)
// is acquired once and kept: the BSP version leaks it when no card answers,
// so a second attempt could never succeed. The ESP32-C6 Wi-Fi link uses slot
// 1 of the same controller; mount, unmount and a failed mount only touch
// slot 0 (the default host config releases just its own slot).

#include <cstdio>
#include <cstring>

#include "bsp/esp-bsp.h"
#include "driver/sdmmc_host.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "sd";
bool s_mounted = false;
char s_error[48] = "";
sdmmc_card_t* s_card = nullptr;
sd_pwr_ctrl_handle_t s_pwr = nullptr;

esp_err_t mountCard() {
  if (s_pwr == nullptr) {
    sd_pwr_ctrl_ldo_config_t ldo = {};
    ldo.ldo_chan_id = 4;
    const esp_err_t err = sd_pwr_ctrl_new_on_chip_ldo(&ldo, &s_pwr);
    if (err != ESP_OK) return err;
  }
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_0;
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
  host.pwr_ctrl_handle = s_pwr;

  sdmmc_slot_config_t slot = {};  // slot 0 uses fixed IO MUX pins
  slot.cd = SDMMC_SLOT_NO_CD;
  slot.wp = SDMMC_SLOT_NO_WP;
  slot.width = 4;

  esp_vfs_fat_sdmmc_mount_config_t mount = {};
  mount.format_if_mount_failed = false;  // never wipe someone's card
  mount.max_files = 5;
  mount.allocation_unit_size = 64 * 1024;
  return esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, &host, &slot, &mount, &s_card);
}

}  // namespace

const char* sdRoot() { return BSP_SD_MOUNT_POINT; }

SdInfo sdInfo() {
  SdInfo info;
  info.mounted = s_mounted;
  std::snprintf(info.error, sizeof(info.error), "%s", s_error);
  if (s_mounted) {
    uint64_t total = 0, free_bytes = 0;
    if (esp_vfs_fat_info(BSP_SD_MOUNT_POINT, &total, &free_bytes) == ESP_OK) {
      info.total_bytes = total;
      info.free_bytes = free_bytes;
    }
  }
  return info;
}

bool sdMount(SdInfo* info) {
  if (!s_mounted) {
    const esp_err_t err = mountCard();
    if (err == ESP_OK) {
      s_mounted = true;
      s_error[0] = '\0';
      ESP_LOGI(TAG, "card mounted at %s", BSP_SD_MOUNT_POINT);
    } else {
      // No card: the card never answers (timeout). Unformatted: FATFS error.
      const char* why = err == ESP_ERR_TIMEOUT || err == ESP_ERR_NOT_FOUND ? "No card"
                        : err == ESP_FAIL                                  ? "Not FAT32 formatted?"
                                                                           : esp_err_to_name(err);
      std::snprintf(s_error, sizeof(s_error), "%s", why);
      ESP_LOGW(TAG, "mount failed: %s", esp_err_to_name(err));
    }
  }
  if (info) *info = sdInfo();
  return s_mounted;
}

void sdUnmount() {
  if (!s_mounted) return;
  esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, s_card);
  s_card = nullptr;
  s_mounted = false;
  std::snprintf(s_error, sizeof(s_error), "Unmounted");
}

}  // namespace plat
