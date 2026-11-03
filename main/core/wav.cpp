#include "core/wav.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace core::wav {

namespace {

uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace

bool decode(const uint8_t* data, size_t size, int rate, double max_seconds, std::vector<int16_t>* out,
            std::string* error) {
  out->clear();
  if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
    *error = "not a WAV file";
    return false;
  }
  int format = 0, channels = 0, bits = 0;
  uint32_t in_rate = 0;
  const uint8_t* samples = nullptr;
  size_t samples_size = 0;
  size_t pos = 12;
  while (pos + 8 <= size) {
    const uint8_t* chunk = data + pos;
    const uint32_t len = u32(chunk + 4);
    const size_t body = pos + 8;
    const size_t avail = size - body;
    if (std::memcmp(chunk, "fmt ", 4) == 0 && len >= 16 && avail >= 16) {
      format = u16(chunk + 8);
      channels = u16(chunk + 10);
      in_rate = u32(chunk + 12);
      bits = u16(chunk + 22);
      if (format == 0xFFFE && len >= 40 && avail >= 40) format = u16(chunk + 32);  // WAVE_FORMAT_EXTENSIBLE: the subformat
    } else if (std::memcmp(chunk, "data", 4) == 0) {
      samples = chunk + 8;
      samples_size = std::min<size_t>(len, avail);  // a cut-off file plays what's there
    }
    if (len > avail) break;  // cut off (or nonsense): nothing follows
    pos = body + len + (len & 1);
    if (samples != nullptr && format != 0) break;
  }
  if (format == 0) {
    *error = "no format chunk";
    return false;
  }
  if (samples == nullptr) {
    *error = "no sound data";
    return false;
  }
  const bool pcm = format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
  const bool flt = format == 3 && bits == 32;
  if (!pcm && !flt) {
    *error = "unsupported format (use PCM or float WAV)";
    return false;
  }
  if (channels < 1 || channels > 8 || in_rate < 2000 || in_rate > 192000 || rate <= 0) {
    *error = "unsupported channels or sample rate";
    return false;
  }
  const size_t frame = static_cast<size_t>(channels) * (bits / 8);
  const size_t frames = samples_size / frame;
  if (frames == 0) {
    *error = "no sound data";
    return false;
  }
  auto sample = [&](size_t f, int ch) -> float {
    const uint8_t* p = samples + f * frame + static_cast<size_t>(ch) * (bits / 8);
    if (flt) {
      float v;
      std::memcpy(&v, p, 4);
      return std::isfinite(v) ? std::max(-1.0f, std::min(1.0f, v)) : 0.0f;
    }
    switch (bits) {
      case 8: return (static_cast<int>(p[0]) - 128) / 128.0f;
      case 16: return static_cast<int16_t>(u16(p)) / 32768.0f;
      case 24: {
        int32_t v = static_cast<int32_t>(p[0] | (p[1] << 8) | (p[2] << 16));
        if (v & 0x800000) v |= ~0xFFFFFF;
        return v / 8388608.0f;
      }
      default: return static_cast<int32_t>(u32(p)) / 2147483648.0f;
    }
  };
  auto mono = [&](size_t f) {
    float s = 0;
    for (int c = 0; c < channels; ++c) s += sample(f, c);
    return s / channels;
  };
  // Linear resampling to the output rate.
  const double step = static_cast<double>(in_rate) / rate;
  size_t n = static_cast<size_t>(frames / step);
  n = std::min(n, static_cast<size_t>(max_seconds * rate));
  out->reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const double x = i * step;
    const size_t a = static_cast<size_t>(x);
    const size_t b = std::min(a + 1, frames - 1);
    const float t = static_cast<float>(x - a);
    const float v = mono(a) * (1 - t) + mono(b) * t;
    out->push_back(static_cast<int16_t>(std::lround(std::max(-1.0f, std::min(1.0f, v)) * 32767.0f)));
  }
  return true;
}

const char* fileName(Sound s) {
  switch (s) {
    case Sound::Click: return "click.wav";
    case Sound::Ping: return "ping.wav";
    case Sound::NewAircraft: return "aircraft_detected.wav";
    case Sound::Alert: return "proximity.wav";
    case Sound::Emergency: return "emergency.wav";
    case Sound::Startup: return "startup.wav";
    case Sound::Home: return "home.wav";
    case Sound::FavoriteSeen: return "favorite_seen.wav";
    case Sound::FavoriteNear: return "favorite_near.wav";
    case Sound::Alarm: return "alarm.wav";
    case Sound::Watch: return "watchlist.wav";
    case Sound::FirstTime: return "first_time.wav";
    default: return nullptr;
  }
}

double maxSeconds(Sound s) {
  switch (s) {
    case Sound::Click:
    case Sound::Ping: return 1.0;  // these come often
    case Sound::Alarm: return 20.0;
    default: return 8.0;
  }
}

}  // namespace core::wav
