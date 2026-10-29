#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace core {

/** The firmware marks its board in the image, so an upload for another board can be refused. */
constexpr char kBoardMarker[] = "PLANE-RADAR-BOARD=";

/**
 * Checks a firmware file as it streams in (uploaded from a phone): an app
 * image of this project (not the full USB image, not some other file), and
 * for this board. Feed every byte in order.
 */
class FirmwareCheck {
 public:
  FirmwareCheck(const char* project_name, const char* board);
  void feed(const uint8_t* data, size_t len);
  /** Bytes needed before headerProblem() can tell. */
  static constexpr size_t kHeaderBytes = 128;
  /** Once kHeaderBytes are in: why the file can't be installed, or "" if it looks right. */
  std::string headerProblem() const;
  /** After the whole file: why it can't be installed, or "". */
  std::string finalProblem() const;
  /** The image's version ("1.18.0"), once the header is in. */
  std::string version() const;
  /** The board the image was built for ("" if it doesn't say: before v1.18). */
  const std::string& imageBoard() const { return found_board_; }

 private:
  std::string project_, board_;
  std::string head_;      // first kHeaderBytes
  std::string tail_;      // carry-over for finding the marker across pieces
  std::string found_board_;
  size_t total_ = 0;
};

}  // namespace core
