#include "core/sound_synth.h"

#include <cmath>

#include "core/geo.h"

namespace core {

namespace {

constexpr double kTwoPi = 2.0 * kPi;

struct Buffer {
  std::vector<float> s;
  int rate;

  explicit Buffer(int r, double seconds) : s(static_cast<size_t>(r * seconds) + 1, 0.0f), rate(r) {}

  /**
   * Adds a tone starting at t0 with exponential decay `tau` and short linear
   * attack. Frequency glides linearly from f0 to f1 over `dur`.
   */
  void tone(double t0, double dur, double f0, double f1, double amp, double tau,
            double attack = 0.004, bool square_ish = false) {
    const size_t start = static_cast<size_t>(t0 * rate);
    const size_t n = static_cast<size_t>(dur * rate);
    double phase = 0;
    for (size_t i = 0; i < n && start + i < s.size(); ++i) {
      const double t = static_cast<double>(i) / rate;
      const double f = f0 + (f1 - f0) * (t / dur);
      phase += kTwoPi * f / rate;
      double env = std::exp(-t / tau);
      if (t < attack) env *= t / attack;
      const double rel = dur - t;  // short release to avoid clicks
      if (rel < 0.006) env *= rel / 0.006;
      double w = std::sin(phase);
      if (square_ish) w = std::tanh(3.0 * w);  // soft-clipped, brighter but not harsh
      s[start + i] += static_cast<float>(amp * env * w);
    }
  }

  std::vector<int16_t> toPcm() const {
    float peak = 0;
    for (float v : s) peak = std::fmax(peak, std::fabs(v));
    const float gain = peak > 0.95f ? 0.95f / peak : 1.0f;
    std::vector<int16_t> out(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
      out[i] = static_cast<int16_t>(std::lround(s[i] * gain * 32767.0f));
    }
    return out;
  }
};

}  // namespace

std::vector<int16_t> synthesize(Sound sound, int rate) {
  switch (sound) {
    case Sound::Click: {
      Buffer b(rate, 0.03);
      b.tone(0, 0.025, 2400, 1800, 0.35, 0.006, 0.001);
      return b.toPcm();
    }
    case Sound::Ping: {
      // Classic sonar: a pure tone with a long tail and a faint echo.
      Buffer b(rate, 1.3);
      b.tone(0.00, 1.0, 1250, 1180, 0.55, 0.28, 0.003);
      b.tone(0.00, 1.0, 2500, 2360, 0.06, 0.12, 0.003);
      b.tone(0.28, 0.9, 1250, 1180, 0.14, 0.22, 0.01);
      return b.toPcm();
    }
    case Sound::NewAircraft: {
      Buffer b(rate, 0.5);
      b.tone(0.00, 0.30, 880, 880, 0.40, 0.12);
      b.tone(0.11, 0.38, 1318.5, 1318.5, 0.40, 0.15);
      return b.toPcm();
    }
    case Sound::Alert: {
      Buffer b(rate, 0.62);
      for (int i = 0; i < 3; ++i) b.tone(i * 0.2, 0.14, 1046.5, 1046.5, 0.5, 0.5, 0.004, true);
      return b.toPcm();
    }
    case Sound::Emergency: {
      Buffer b(rate, 1.65);
      for (int i = 0; i < 4; ++i) {
        b.tone(i * 0.4, 0.2, 960, 1280, 0.5, 1.0, 0.005, true);
        b.tone(i * 0.4 + 0.2, 0.2, 1280, 960, 0.5, 1.0, 0.005, true);
      }
      return b.toPcm();
    }
    case Sound::Startup: {
      Buffer b(rate, 1.0);
      const double notes[] = {523.25, 659.25, 783.99, 1046.5};
      for (int i = 0; i < 4; ++i) b.tone(i * 0.09, 0.6 - i * 0.05, notes[i], notes[i], 0.3, 0.18);
      return b.toPcm();
    }
    case Sound::Home: {
      Buffer b(rate, 0.4);
      b.tone(0, 0.35, 500, 1400, 0.35, 0.2, 0.02);
      return b.toPcm();
    }
    case Sound::FavoriteSeen: {
      Buffer b(rate, 0.9);
      const double notes[] = {783.99, 987.77, 1318.51};  // G5 B5 E6
      for (int i = 0; i < 3; ++i) b.tone(i * 0.12, 0.55, notes[i], notes[i], 0.38, 0.22);
      b.tone(0.36, 0.5, 2637.0, 2637.0, 0.08, 0.15);  // sparkle
      return b.toPcm();
    }
    case Sound::FavoriteNear: {
      Buffer b(rate, 1.45);
      for (int i = 0; i < 2; ++i) {
        b.tone(i * 0.7, 0.4, 1174.66, 1174.66, 0.45, 0.25);         // D6
        b.tone(i * 0.7 + 0.22, 0.45, 880.0, 880.0, 0.45, 0.28);     // A5
      }
      return b.toPcm();
    }
    default:
      return {};
  }
}

}  // namespace core
