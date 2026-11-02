#include "core/job_queue.h"

#include <chrono>

namespace core {

bool JobQueue::push(std::function<void()> job, size_t bytes, bool droppable) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (droppable && bytes_ + bytes > max_bytes_) {
      ++dropped_;
      return false;
    }
    jobs_.push_back({std::move(job), bytes});
    bytes_ += bytes;
  }
  cv_.notify_one();
  return true;
}

bool JobQueue::runOne() {
  Job job;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (jobs_.empty()) return false;
    job = std::move(jobs_.front());
    jobs_.pop_front();
    running_ = true;
  }
  if (job.fn) job.fn();
  std::lock_guard<std::mutex> lock(mu_);
  bytes_ -= job.bytes;  // counted until done: the data lives in the job until then
  running_ = false;
  return true;
}

bool JobQueue::waitForJob(uint32_t ms) {
  std::unique_lock<std::mutex> lock(mu_);
  return cv_.wait_for(lock, std::chrono::milliseconds(ms), [this] { return !jobs_.empty(); });
}

size_t JobQueue::pending() const {
  std::lock_guard<std::mutex> lock(mu_);
  return jobs_.size();
}

size_t JobQueue::bytes() const {
  std::lock_guard<std::mutex> lock(mu_);
  return bytes_;
}

uint32_t JobQueue::dropped() const {
  std::lock_guard<std::mutex> lock(mu_);
  return dropped_;
}

bool JobQueue::busy() const {
  std::lock_guard<std::mutex> lock(mu_);
  return running_;
}

}  // namespace core
