// Minimal DEFLATE decoder (RFC 1951) with a gzip wrapper (RFC 1952). Small
// and slow-ish, which is fine for the occasional trace file of a few hundred
// kilobytes. Canonical Huffman decoding as in zlib's "puff".

#include "core/inflate.h"

#include <cstring>

namespace core {

namespace {

constexpr int kMaxBits = 15;
constexpr int kMaxLitLen = 288;
constexpr int kMaxDist = 30;

struct Huffman {
  uint16_t count[kMaxBits + 1];  // number of codes of each length
  uint16_t symbol[kMaxLitLen];   // symbols ordered by code
};

class Inflater {
 public:
  Inflater(const uint8_t* in, size_t len, std::string* out, size_t max_out)
      : in_(in), len_(len), out_(out), max_out_(max_out) {}

  bool run() {
    for (;;) {
      const int last = bits(1);
      const int type = bits(2);
      if (err_) return false;
      bool ok;
      switch (type) {
        case 0: ok = stored(); break;
        case 1: ok = fixedBlock(); break;
        case 2: ok = dynamicBlock(); break;
        default: return false;
      }
      if (!ok || err_) return false;
      if (last) return true;
    }
  }

  size_t consumed() const { return pos_; }

 private:
  int bits(int need) {
    uint32_t val = bitbuf_;
    while (bitcnt_ < need) {
      if (pos_ >= len_) {
        err_ = true;
        return 0;
      }
      val |= static_cast<uint32_t>(in_[pos_++]) << bitcnt_;
      bitcnt_ += 8;
    }
    bitbuf_ = val >> need;
    bitcnt_ -= need;
    return static_cast<int>(val & ((1u << need) - 1));
  }

  bool put(char c) {
    if (out_->size() >= max_out_) return false;
    out_->push_back(c);
    return true;
  }

  bool stored() {
    bitbuf_ = 0;
    bitcnt_ = 0;
    if (pos_ + 4 > len_) return false;
    const unsigned n = in_[pos_] | (in_[pos_ + 1] << 8);
    const unsigned nn = in_[pos_ + 2] | (in_[pos_ + 3] << 8);
    pos_ += 4;
    if (n != (~nn & 0xFFFFu) || pos_ + n > len_ || out_->size() + n > max_out_) return false;
    out_->append(reinterpret_cast<const char*>(in_ + pos_), n);
    pos_ += n;
    return true;
  }

  int decode(const Huffman& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= kMaxBits; ++len) {
      code |= bits(1);
      if (err_) return -1;
      const int count = h.count[len];
      if (code - count < first) return h.symbol[index + (code - first)];
      index += count;
      first += count;
      first <<= 1;
      code <<= 1;
    }
    return -1;
  }

  // Returns false for an over-subscribed set of lengths. Incomplete sets are
  // allowed (single-code distance trees are legal).
  static bool build(Huffman* h, const uint8_t* length, int n) {
    std::memset(h->count, 0, sizeof(h->count));
    for (int s = 0; s < n; ++s) h->count[length[s]]++;
    if (h->count[0] == n) return true;
    int left = 1;
    for (int len = 1; len <= kMaxBits; ++len) {
      left <<= 1;
      left -= h->count[len];
      if (left < 0) return false;
    }
    uint16_t offs[kMaxBits + 1];
    offs[1] = 0;
    for (int len = 1; len < kMaxBits; ++len) offs[len + 1] = offs[len] + h->count[len];
    for (int s = 0; s < n; ++s) {
      if (length[s] != 0) h->symbol[offs[length[s]]++] = static_cast<uint16_t>(s);
    }
    return true;
  }

  bool codes(const Huffman& lencode, const Huffman& distcode) {
    static const uint16_t kLBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t kLExt[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t kDBase[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,
                                        33,  49,  65,  97,  129, 193,  257,  385,  513,  769,
                                        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t kDExt[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
      int sym = decode(lencode);
      if (sym < 0) return false;
      if (sym < 256) {
        if (!put(static_cast<char>(sym))) return false;
      } else if (sym == 256) {
        return true;
      } else {
        sym -= 257;
        if (sym >= 29) return false;
        const size_t len = kLBase[sym] + bits(kLExt[sym]);
        const int dsym = decode(distcode);
        if (dsym < 0 || dsym >= 30) return false;
        const size_t dist = kDBase[dsym] + bits(kDExt[dsym]);
        if (err_ || dist > out_->size() || out_->size() + len > max_out_) return false;
        const size_t from = out_->size() - dist;
        for (size_t i = 0; i < len; ++i) out_->push_back((*out_)[from + i]);
      }
    }
  }

  bool fixedBlock() {
    static Huffman lencode, distcode;
    static bool built = false;
    if (!built) {
      uint8_t lengths[kMaxLitLen];
      int s = 0;
      for (; s < 144; ++s) lengths[s] = 8;
      for (; s < 256; ++s) lengths[s] = 9;
      for (; s < 280; ++s) lengths[s] = 7;
      for (; s < kMaxLitLen; ++s) lengths[s] = 8;
      build(&lencode, lengths, kMaxLitLen);
      for (s = 0; s < kMaxDist; ++s) lengths[s] = 5;
      build(&distcode, lengths, kMaxDist);
      built = true;
    }
    return codes(lencode, distcode);
  }

  bool dynamicBlock() {
    static const uint8_t kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int nlen = bits(5) + 257;
    const int ndist = bits(5) + 1;
    const int ncode = bits(4) + 4;
    if (err_ || nlen > 286 || ndist > kMaxDist) return false;

    uint8_t lengths[kMaxLitLen + kMaxDist];
    std::memset(lengths, 0, sizeof(lengths));
    for (int i = 0; i < ncode; ++i) lengths[kOrder[i]] = static_cast<uint8_t>(bits(3));
    Huffman lencode, distcode;
    if (err_ || !build(&lencode, lengths, 19)) return false;

    int index = 0;
    while (index < nlen + ndist) {
      int sym = decode(lencode);
      if (sym < 0) return false;
      if (sym < 16) {
        lengths[index++] = static_cast<uint8_t>(sym);
        continue;
      }
      uint8_t len = 0;
      int repeat;
      if (sym == 16) {
        if (index == 0) return false;
        len = lengths[index - 1];
        repeat = 3 + bits(2);
      } else if (sym == 17) {
        repeat = 3 + bits(3);
      } else {
        repeat = 11 + bits(7);
      }
      if (err_ || index + repeat > nlen + ndist) return false;
      while (repeat--) lengths[index++] = len;
    }
    if (lengths[256] == 0) return false;
    if (!build(&lencode, lengths, nlen) || !build(&distcode, lengths + nlen, ndist)) return false;
    return codes(lencode, distcode);
  }

  const uint8_t* in_;
  size_t len_;
  size_t pos_ = 0;
  uint32_t bitbuf_ = 0;
  int bitcnt_ = 0;
  bool err_ = false;
  std::string* out_;
  size_t max_out_;
};

}  // namespace

bool isGzip(const void* data, size_t len) {
  const auto* p = static_cast<const uint8_t*>(data);
  return len >= 2 && p[0] == 0x1f && p[1] == 0x8b;
}

bool inflateRaw(const void* data, size_t len, std::string* out, size_t max_out) {
  Inflater inf(static_cast<const uint8_t*>(data), len, out, max_out);
  return inf.run();
}

bool gunzip(const void* data, size_t len, std::string* out, size_t max_out) {
  const auto* p = static_cast<const uint8_t*>(data);
  if (len < 18 || !isGzip(data, len) || p[2] != 8) return false;  // 8 = deflate
  const uint8_t flags = p[3];
  size_t pos = 10;
  if (flags & 0x04) {  // FEXTRA
    if (pos + 2 > len) return false;
    pos += 2 + (p[pos] | (p[pos + 1] << 8));
  }
  if (flags & 0x08) {  // FNAME
    while (pos < len && p[pos] != 0) ++pos;
    ++pos;
  }
  if (flags & 0x10) {  // FCOMMENT
    while (pos < len && p[pos] != 0) ++pos;
    ++pos;
  }
  if (flags & 0x02) pos += 2;  // FHCRC
  if (pos >= len) return false;
  out->clear();
  return inflateRaw(p + pos, len - pos, out, max_out);
}

}  // namespace core
