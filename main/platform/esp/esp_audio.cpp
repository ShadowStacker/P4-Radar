// Sound effects through the ES8311 codec and on-board speaker amp. Effects
// are synthesized once at boot (core/sound_synth) and played by a small task.

#include <atomic>
#include <vector>

#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "platform/esp/esp_platform.h"
#include "platform/platform.h"

namespace plat {

namespace {

const char* TAG = "audio";
constexpr int kSampleRate = 22050;  // BSP's default I2S config

esp_codec_dev_handle_t s_spk = nullptr;
QueueHandle_t s_queue = nullptr;
std::vector<int16_t> s_clips[static_cast<int>(core::Sound::kCount)];
std::atomic<int> s_volume{60};

void audioTask(void*) {
  uint8_t id;
  for (;;) {
    if (xQueueReceive(s_queue, &id, portMAX_DELAY) != pdTRUE) continue;
    const std::vector<int16_t>& clip = s_clips[id];
    if (clip.empty() || s_volume.load() == 0) continue;
    esp_codec_dev_write(s_spk, const_cast<int16_t*>(clip.data()), static_cast<int>(clip.size() * sizeof(int16_t)));
  }
}

}  // namespace

namespace esp {

void audioStart() {
  s_spk = bsp_audio_codec_speaker_init();
  if (s_spk == nullptr) {
    ESP_LOGE(TAG, "speaker codec not available - sounds disabled");
    return;
  }
  esp_codec_dev_sample_info_t fs = {};
  fs.bits_per_sample = 16;
  fs.channel = 1;
  fs.sample_rate = kSampleRate;
  if (esp_codec_dev_open(s_spk, &fs) != ESP_CODEC_DEV_OK) {
    ESP_LOGE(TAG, "codec open failed");
    s_spk = nullptr;
    return;
  }
  esp_codec_dev_set_out_vol(s_spk, s_volume.load());

  for (int i = 0; i < static_cast<int>(core::Sound::kCount); ++i) {
    s_clips[i] = core::synthesize(static_cast<core::Sound>(i), kSampleRate);
  }
  s_queue = xQueueCreate(4, sizeof(uint8_t));
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 6, nullptr, 0);
}

}  // namespace esp

void playSound(core::Sound s) {
  if (s_queue == nullptr) return;
  const uint8_t id = static_cast<uint8_t>(s);
  xQueueSend(s_queue, &id, 0);  // drop if a burst is already queued
}

void setVolume(int percent) {
  s_volume = percent;
  if (s_spk != nullptr) esp_codec_dev_set_out_vol(s_spk, percent);
}

}  // namespace plat
