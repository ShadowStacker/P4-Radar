#include "core/png.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "core/inflate.h"

namespace core::png {

namespace {

uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

int paeth(int a, int b, int c) {
  const int p = a + b - c;
  const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  return pb <= pc ? b : c;
}

}  // namespace

bool decode(const uint8_t* data, size_t len, Image* out, int max_pixels) {
  static const uint8_t kSig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  if (len < 8 || std::memcmp(data, kSig, 8) != 0) return false;

  int w = 0, h = 0, depth = 0, color = -1, interlace = 0;
  std::string idat;
  uint8_t palette[256][4];
  int palette_n = 0;
  for (int i = 0; i < 256; ++i) palette[i][3] = 255;

  size_t pos = 8;
  bool have_ihdr = false;
  while (pos + 12 <= len) {
    const uint32_t n = be32(data + pos);
    const char* type = reinterpret_cast<const char*>(data + pos + 4);
    const uint8_t* body = data + pos + 8;
    if (n > len - pos - 12) return false;
    if (std::memcmp(type, "IHDR", 4) == 0 && n >= 13) {
      w = static_cast<int>(be32(body));
      h = static_cast<int>(be32(body + 4));
      depth = body[8];
      color = body[9];
      interlace = body[12];
      have_ihdr = true;
    } else if (std::memcmp(type, "PLTE", 4) == 0) {
      palette_n = static_cast<int>(n / 3) > 256 ? 256 : static_cast<int>(n / 3);
      for (int i = 0; i < palette_n; ++i) {
        palette[i][0] = body[i * 3];
        palette[i][1] = body[i * 3 + 1];
        palette[i][2] = body[i * 3 + 2];
      }
    } else if (std::memcmp(type, "tRNS", 4) == 0 && color == 3) {
      for (uint32_t i = 0; i < n && i < 256; ++i) palette[i][3] = body[i];
    } else if (std::memcmp(type, "IDAT", 4) == 0) {
      idat.append(reinterpret_cast<const char*>(body), n);
    } else if (std::memcmp(type, "IEND", 4) == 0) {
      break;
    }
    pos += 12 + n;  // length + type + data + CRC (CRC unchecked: TLS already protects the bytes)
  }
  if (!have_ihdr || w <= 0 || h <= 0 || static_cast<long long>(w) * h > max_pixels || interlace != 0) return false;

  int channels;
  switch (color) {
    case 0: channels = 1; break;  // gray
    case 2: channels = 3; break;  // RGB
    case 3: channels = 1; break;  // palette index
    case 4: channels = 2; break;  // gray + alpha
    case 6: channels = 4; break;  // RGBA
    default: return false;
  }
  const bool sub_byte = color == 3 && (depth == 1 || depth == 2 || depth == 4);
  if (!(depth == 8 || sub_byte)) return false;
  if (color == 3 && palette_n == 0) return false;

  const int bits_pp = channels * depth;
  const size_t row_bytes = (static_cast<size_t>(w) * bits_pp + 7) / 8;
  const int bpp = bits_pp >= 8 ? bits_pp / 8 : 1;  // filter byte distance

  // zlib stream: 2-byte header, deflate data, 4-byte Adler-32.
  if (idat.size() < 6 || (static_cast<uint8_t>(idat[0]) & 0x0F) != 8) return false;
  std::string raw;
  const size_t expected = (row_bytes + 1) * h;
  raw.reserve(expected);
  if (!core::inflateRaw(idat.data() + 2, idat.size() - 2, &raw, expected) || raw.size() < expected) return false;

  // Undo the per-row filters in place.
  auto* px = reinterpret_cast<uint8_t*>(&raw[0]);
  const uint8_t* prev = nullptr;
  for (int y = 0; y < h; ++y) {
    uint8_t* row = px + y * (row_bytes + 1);
    const uint8_t filter = row[0];
    uint8_t* cur = row + 1;
    for (size_t i = 0; i < row_bytes; ++i) {
      const int a = i >= static_cast<size_t>(bpp) ? cur[i - bpp] : 0;
      const int b = prev ? prev[i] : 0;
      const int c = (prev && i >= static_cast<size_t>(bpp)) ? prev[i - bpp] : 0;
      switch (filter) {
        case 0: break;
        case 1: cur[i] = static_cast<uint8_t>(cur[i] + a); break;
        case 2: cur[i] = static_cast<uint8_t>(cur[i] + b); break;
        case 3: cur[i] = static_cast<uint8_t>(cur[i] + ((a + b) >> 1)); break;
        case 4: cur[i] = static_cast<uint8_t>(cur[i] + paeth(a, b, c)); break;
        default: return false;
      }
    }
    prev = cur;
  }

  out->width = w;
  out->height = h;
  out->rgba.resize(static_cast<size_t>(w) * h * 4);
  uint8_t* o = out->rgba.data();
  for (int y = 0; y < h; ++y) {
    const uint8_t* s = px + y * (row_bytes + 1) + 1;
    for (int x = 0; x < w; ++x, o += 4) {
      switch (color) {
        case 0: o[0] = o[1] = o[2] = s[x]; o[3] = 255; break;
        case 2: o[0] = s[x * 3]; o[1] = s[x * 3 + 1]; o[2] = s[x * 3 + 2]; o[3] = 255; break;
        case 4: o[0] = o[1] = o[2] = s[x * 2]; o[3] = s[x * 2 + 1]; break;
        case 6: std::memcpy(o, s + x * 4, 4); break;
        case 3: {
          int idx;
          if (depth == 8) {
            idx = s[x];
          } else {
            const int per_byte = 8 / depth;
            const int shift = (per_byte - 1 - x % per_byte) * depth;
            idx = (s[x / per_byte] >> shift) & ((1 << depth) - 1);
          }
          if (idx >= palette_n) idx = 0;
          std::memcpy(o, palette[idx], 4);
          break;
        }
      }
    }
  }
  return true;
}

}  // namespace core::png
