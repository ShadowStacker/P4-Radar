#include "core/deflate.h"

#include <algorithm>
#include <array>
#include <vector>

namespace core {

namespace {

constexpr uint16_t kLenBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr uint16_t kDistBase[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

constexpr int kWindow = 32768;
constexpr int kHashBits = 15;
constexpr int kMaxChain = 8;
constexpr int kMinMatch = 3, kMaxMatch = 258;

class BitWriter {
 public:
  explicit BitWriter(std::string* out) : out_(out) {}
  void bits(uint32_t v, int n) {  // LSB first
    acc_ |= static_cast<uint64_t>(v) << n_;
    n_ += n;
    while (n_ >= 8) {
      out_->push_back(static_cast<char>(acc_ & 0xFF));
      acc_ >>= 8;
      n_ -= 8;
    }
  }
  void code(uint32_t c, int n) {  // Huffman codes go MSB first
    uint32_t r = 0;
    for (int i = 0; i < n; ++i) r |= ((c >> i) & 1u) << (n - 1 - i);
    bits(r, n);
  }
  void flush() {
    if (n_ > 0) out_->push_back(static_cast<char>(acc_ & 0xFF));
    acc_ = 0;
    n_ = 0;
  }

 private:
  std::string* out_;
  uint64_t acc_ = 0;
  int n_ = 0;
};

void literal(BitWriter& w, int v) {  // 0-287, fixed code
  if (v < 144) w.code(0x30 + v, 8);
  else if (v < 256) w.code(0x190 + (v - 144), 9);
  else if (v < 280) w.code(v - 256, 7);
  else w.code(0xC0 + (v - 280), 8);
}

void match(BitWriter& w, int len, int dist) {
  int li = 28;
  while (kLenBase[li] > len) --li;
  literal(w, 257 + li);
  if (kLenExtra[li]) w.bits(static_cast<uint32_t>(len - kLenBase[li]), kLenExtra[li]);
  int di = 29;
  while (kDistBase[di] > dist) --di;
  w.code(static_cast<uint32_t>(di), 5);
  if (kDistExtra[di]) w.bits(static_cast<uint32_t>(dist - kDistBase[di]), kDistExtra[di]);
}

inline uint32_t hash3(const uint8_t* p) {
  return ((static_cast<uint32_t>(p[0]) << 16 | static_cast<uint32_t>(p[1]) << 8 | p[2]) * 2654435761u) >>
         (32 - kHashBits);
}

}  // namespace

std::string deflateRaw(const uint8_t* data, size_t len) {
  std::string out;
  out.reserve(len / 4 + 64);
  BitWriter w(&out);
  w.bits(1, 1);  // BFINAL
  w.bits(1, 2);  // fixed Huffman codes
  std::vector<int32_t> head(1u << kHashBits, -1);
  std::vector<int32_t> prev(kWindow, -1);
  size_t i = 0;
  auto insert = [&](size_t pos) {
    if (pos + kMinMatch > len) return;
    const uint32_t h = hash3(data + pos);
    prev[pos % kWindow] = head[h];
    head[h] = static_cast<int32_t>(pos);
  };
  while (i < len) {
    int best_len = 0, best_dist = 0;
    if (i + kMinMatch <= len) {
      int32_t cand = head[hash3(data + i)];
      const size_t max_len = std::min<size_t>(kMaxMatch, len - i);
      for (int chain = 0; cand >= 0 && chain < kMaxChain; ++chain) {
        const size_t dist = i - static_cast<size_t>(cand);
        if (dist > kWindow - 1 || dist == 0) break;
        const uint8_t* a = data + cand;
        const uint8_t* b = data + i;
        size_t n = 0;
        while (n < max_len && a[n] == b[n]) ++n;
        if (static_cast<int>(n) > best_len) {
          best_len = static_cast<int>(n);
          best_dist = static_cast<int>(dist);
          if (n == max_len) break;
        }
        const int32_t next = prev[cand % kWindow];
        if (next >= cand) break;
        cand = next;
      }
    }
    if (best_len >= kMinMatch) {
      match(w, best_len, best_dist);
      // Long runs: index only the start, so huge flat areas stay fast.
      const size_t step_end = i + static_cast<size_t>(best_len);
      const size_t index_end = best_len > 32 ? i + 4 : step_end;
      for (size_t k = i; k < index_end; ++k) insert(k);
      i = step_end;
    } else {
      literal(w, data[i]);
      insert(i);
      ++i;
    }
  }
  literal(w, 256);  // end of block
  w.flush();
  return out;
}

uint32_t adler32(uint32_t adler, const uint8_t* data, size_t len) {
  uint32_t a = adler & 0xFFFF, b = adler >> 16;
  while (len > 0) {
    size_t n = len < 5552 ? len : 5552;
    len -= n;
    while (n--) {
      a += *data++;
      b += a;
    }
    a %= 65521;
    b %= 65521;
  }
  return b << 16 | a;
}

uint32_t crc32(uint32_t crc, const void* data, size_t len) {
  static const std::array<uint32_t, 256> table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t n = 0; n < 256; ++n) {
      uint32_t c = n;
      for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      t[n] = c;
    }
    return t;
  }();
  const auto* p = static_cast<const uint8_t*>(data);
  crc = ~crc;
  while (len--) crc = table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

std::string zlibCompress(const uint8_t* data, size_t len) {
  std::string out("\x78\x01", 2);
  out += deflateRaw(data, len);
  const uint32_t a = adler32(1, data, len);
  for (int s = 24; s >= 0; s -= 8) out.push_back(static_cast<char>(a >> s));
  return out;
}

}  // namespace core
