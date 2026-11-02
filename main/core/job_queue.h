#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>

namespace core {

/**
 * Work handed from one task to another and run there in order, one job at a
 * time (used for SD card writes, so the network task never waits for the
 * card). Thread-safe; one consumer.
 */
class JobQueue {
 public:
  explicit JobQueue(size_t max_bytes) : max_bytes_(max_bytes) {}

  /**
   * Adds a job holding about `bytes` of data. A `droppable` job (a cache
   * write that can be redone) is refused while the queue already holds
   * max_bytes; others always go in. False if refused.
   */
  bool push(std::function<void()> job, size_t bytes, bool droppable);

  /** Runs the oldest job; false if there was none. */
  bool runOne();

  /** Waits up to `ms` for a job to arrive; true if there's one. */
  bool waitForJob(uint32_t ms);

  size_t pending() const;
  size_t bytes() const;
  uint32_t dropped() const;
  /** Jobs taken out but not finished yet (0 or 1). */
  bool busy() const;

 private:
  struct Job {
    std::function<void()> fn;
    size_t bytes;
  };
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<Job> jobs_;
  size_t bytes_ = 0;
  size_t max_bytes_;
  uint32_t dropped_ = 0;
  bool running_ = false;
};

}  // namespace core
