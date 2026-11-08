#pragma once

#include <cstddef>
#include <cstdint>

namespace core {

/**
 * The last 24 hours of free memory, in half-hour slots: the lowest free
 * internal RAM and PSRAM seen in each. A slow leak shows as a falling line
 * (Settings > About), without a serial cable.
 */
class MemHistory {
 public:
  static constexpr int kSlots = 48;
  static constexpr uint32_t kSlotMs = 30u * 60u * 1000u;

  struct Slot {
    uint32_t ram_min = 0;    // bytes
    uint32_t psram_min = 0;  // bytes
  };

  /** A reading (call every few seconds). */
  void sample(uint32_t now_ms, uint32_t ram_free, uint32_t psram_free) {
    if (!started_) {
      started_ = true;
      slot_start_ms_ = now_ms;
      cur_ = {ram_free, psram_free};
      return;
    }
    while (now_ms - slot_start_ms_ >= kSlotMs) {  // close the slot (and any skipped ones, as copies)
      push(cur_);
      slot_start_ms_ += kSlotMs;
      cur_ = {ram_free, psram_free};
    }
    if (ram_free < cur_.ram_min) cur_.ram_min = ram_free;
    if (psram_free < cur_.psram_min) cur_.psram_min = psram_free;
  }

  /** Slots so far, the current (unfinished) one included; at most kSlots. */
  int count() const { return started_ ? (done_ < kSlots ? done_ + 1 : kSlots) : 0; }

  /** Slot i, oldest first (i = count() - 1 is the current one). */
  Slot at(int i) const {
    const int n = count();
    if (i < 0 || i >= n) return {};
    if (i == n - 1) return cur_;
    const int finished = n - 1;  // the finished slots shown
    return ring_[(head_ + kSlots - finished + i) % kSlots];
  }

 private:
  void push(const Slot& s) {
    ring_[head_] = s;
    head_ = (head_ + 1) % kSlots;
    if (done_ < kSlots) ++done_;
  }

  Slot ring_[kSlots];
  int head_ = 0;   // next write
  int done_ = 0;   // finished slots stored
  bool started_ = false;
  uint32_t slot_start_ms_ = 0;
  Slot cur_;
};

}  // namespace core
