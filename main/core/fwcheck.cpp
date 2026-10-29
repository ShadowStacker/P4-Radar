#include "core/fwcheck.h"

#include <algorithm>
#include <cstring>

namespace core {

namespace {
// esp_image_header_t (24 bytes) + first segment header (8), then esp_app_desc_t.
constexpr size_t kDescAt = 32;
constexpr uint32_t kDescMagic = 0xABCD5432;
constexpr size_t kVersionAt = kDescAt + 16, kProjectAt = kDescAt + 48;
constexpr size_t kMaxBoard = 40;

std::string cstr(const std::string& s, size_t at, size_t max) {
  if (at >= s.size()) return "";
  const size_t n = std::min(max, s.size() - at);
  const size_t end = s.find('\0', at);
  return s.substr(at, (end == std::string::npos ? at + n : std::min(end, at + n)) - at);
}
}  // namespace

FirmwareCheck::FirmwareCheck(const char* project_name, const char* board) : project_(project_name), board_(board) {}

void FirmwareCheck::feed(const uint8_t* data, size_t len) {
  total_ += len;
  if (head_.size() < kHeaderBytes) {
    head_.append(reinterpret_cast<const char*>(data), std::min(len, kHeaderBytes - head_.size()));
  }
  if (!found_board_.empty()) return;
  std::string buf = tail_;
  buf.append(reinterpret_cast<const char*>(data), len);
  const size_t mlen = sizeof(kBoardMarker) - 1;
  // The checker's own copy of the marker (with nothing after it) is in every
  // image too: only a marker followed by a name counts.
  size_t from = 0;
  for (;;) {
    const size_t m = buf.find(kBoardMarker, from);
    if (m == std::string::npos) break;
    const size_t end = buf.find('\0', m + mlen);
    if (end == std::string::npos) {
      tail_ = buf.substr(m);  // the name continues in the next piece
      if (tail_.size() > mlen + kMaxBoard) tail_.clear();
      return;
    }
    if (end > m + mlen) {
      found_board_ = buf.substr(m + mlen, std::min(end - m - mlen, kMaxBoard));
      tail_.clear();
      return;
    }
    from = m + mlen;
  }
  const size_t keep = mlen + kMaxBoard;
  tail_ = buf.size() > keep ? buf.substr(buf.size() - keep) : buf;
}

std::string FirmwareCheck::headerProblem() const {
  if (head_.size() < kHeaderBytes) return total_ == 0 ? "The file is empty" : "That file is too small to be firmware";
  const auto* h = reinterpret_cast<const uint8_t*>(head_.data());
  if (h[0] != 0xE9) {
    if (h[0] == 0xFF) return "That's the full image for flashing over USB. Choose the file that ends in -ota.bin";
    return "That file isn't Plane Radar firmware";
  }
  const uint32_t magic = static_cast<uint32_t>(h[kDescAt]) | static_cast<uint32_t>(h[kDescAt + 1]) << 8 |
                         static_cast<uint32_t>(h[kDescAt + 2]) << 16 | static_cast<uint32_t>(h[kDescAt + 3]) << 24;
  if (magic != kDescMagic) return "That file isn't an app image (a bootloader?). Choose the file that ends in -ota.bin";
  if (cstr(head_, kProjectAt, 32) != project_) return "That file isn't Plane Radar firmware";
  return "";
}

std::string FirmwareCheck::finalProblem() const {
  std::string p = headerProblem();
  if (!p.empty()) return p;
  if (!found_board_.empty() && found_board_ != board_) {
    return "That file is for the " + found_board_ + " board; this one is " + board_;
  }
  return "";
}

std::string FirmwareCheck::version() const { return cstr(head_, kVersionAt, 32); }

}  // namespace core
