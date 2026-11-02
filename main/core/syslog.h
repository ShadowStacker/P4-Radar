#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * The system log: everything the firmware prints (its own messages and
 * ESP-IDF's), kept in RAM for the phone page and saved to the SD card
 * (log/system.log) now and then. Thread-safe; append() may be called from
 * any task (not from an interrupt).
 */
namespace core::syslog {

constexpr size_t kRecentBytes = 24 * 1024;   // kept for the phone page
constexpr size_t kPendingBytes = 32 * 1024;  // waiting for the card, at most
constexpr size_t kFileBytes = 512 * 1024;    // system.log moves to system.1.log past this

/**
 * Adds printed text. `stamp` (e.g. "14:03:22 ") starts each new line.
 * Terminal color codes are dropped; carriage returns too.
 */
void append(const char* text, size_t len, const char* stamp);

/** The latest text (whole lines, oldest first), up to kRecentBytes. */
std::string recent();

/**
 * Moves out the text not given out yet (for the card). If more than
 * kPendingBytes piled up, the oldest is gone and a note says so.
 */
std::string takePending();

/** Total bytes logged since the start (for tests and the About page). */
uint64_t totalBytes();

/** Forgets everything (tests). */
void reset();

/**
 * Also hands each finished line (with its stamp and '\n') to `sink`, e.g.
 * to keep the last lines where they survive a crash. Called with the log's
 * lock held: it must not log.
 */
void setLineSink(void (*sink)(const char* line, size_t len));

}  // namespace core::syslog
