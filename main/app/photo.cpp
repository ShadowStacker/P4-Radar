// Photo of the tapped aircraft, from planespotters.net. Their terms: credit
// the photographer next to it, use the URLs as given, don't store the images
// (they're only kept in memory while shown).

#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

#include "app/app.h"
#include "core/jpeg.h"
#include "core/photo.h"
#include "platform/platform.h"

namespace app {

namespace {

std::mutex s_mu;  // guards everything below
std::string s_wanted;         // hex of the latest request ("" = none)
bool s_pending = false;
Photo s_ready;                // decoded, waiting for the UI
struct Known {
  std::string hex;
  core::photo::Info info;     // empty image_url = no photo
  uint32_t when_ms;
};
std::deque<Known> s_known;    // recent lookups (planespotters allows caching for a day)
constexpr size_t kKnownCount = 16;
constexpr uint32_t kKnownForMs = 6 * 60 * 60 * 1000;

uint16_t* toRgb565(const core::png::Image& img) {
  auto* px = static_cast<uint16_t*>(plat::allocLarge(static_cast<size_t>(img.width) * img.height * 2));
  if (px == nullptr) return nullptr;
  for (int i = 0; i < img.width * img.height; ++i) {
    const uint8_t* p = &img.rgba[static_cast<size_t>(i) * 4];
    px[i] = static_cast<uint16_t>((p[0] & 0xF8) << 8 | (p[1] & 0xFC) << 3 | p[2] >> 3);
  }
  return px;
}

}  // namespace

void requestPhoto(const char* hex) {
  if (!settings().show_photos) hex = "";
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_wanted == hex) return;
  s_wanted = hex;
  s_pending = !s_wanted.empty();
  if (s_ready.pixels != nullptr) {  // for an earlier aircraft
    plat::freeLarge(s_ready.pixels);
    s_ready = Photo{};
  }
}

bool popPhoto(Photo* out) {
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_ready.pixels == nullptr || s_wanted != s_ready.hex) return false;
  *out = s_ready;
  s_ready = Photo{};
  return true;
}

bool servicePhoto() {
  std::string hex;
  core::photo::Info info;
  bool known = false;
  {
    std::lock_guard<std::mutex> lock(s_mu);
    if (!s_pending) return false;
    s_pending = false;
    hex = s_wanted;
    for (const Known& k : s_known) {
      if (k.hex == hex && plat::millis() - k.when_ms < kKnownForMs) {
        info = k.info;
        known = true;
      }
    }
  }
  if (!known) {
    std::string body;
    const int code = plat::httpGet(core::photo::lookupUrl(hex.c_str()).c_str(), &body, 10000);
    const bool found = code == 200 && core::photo::parse(body.data(), body.size(), &info);
    std::printf("photo: %s -> HTTP %d%s\n", hex.c_str(), code, found ? "" : " (none)");
    if (code != 200 && code != 404) return true;  // try again on the next tap
    std::lock_guard<std::mutex> lock(s_mu);
    s_known.push_front({hex, info, plat::millis()});
    if (s_known.size() > kKnownCount) s_known.pop_back();
  }
  if (info.image_url.empty()) return true;  // no photo of this one

  std::string jpg;
  const int code = plat::httpGet(info.image_url.c_str(), &jpg, 10000);
  core::png::Image img;
  if (code != 200 || !core::jpeg::decode(reinterpret_cast<const uint8_t*>(jpg.data()), jpg.size(), &img, 700 * 500)) {
    std::printf("photo: %s -> HTTP %d, not a JPEG we can show\n", info.image_url.c_str(), code);
    return true;
  }
  Photo p;
  std::snprintf(p.hex, sizeof(p.hex), "%s", hex.c_str());
  p.width = img.width;
  p.height = img.height;
  p.pixels = toRgb565(img);
  std::snprintf(p.credit, sizeof(p.credit), "Photo: %s / planespotters.net",
                info.photographer.empty() ? "unknown" : info.photographer.c_str());
  if (p.pixels == nullptr) return true;
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_wanted != hex) {  // the selection moved on meanwhile
    plat::freeLarge(p.pixels);
    return true;
  }
  if (s_ready.pixels != nullptr) plat::freeLarge(s_ready.pixels);
  s_ready = p;
  return true;
}

}  // namespace app
