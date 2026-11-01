#include "core/replay.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "core/deflate.h"
#include "core/inflate.h"

namespace core::replay {

namespace {

constexpr uint32_t kMagic = 0x31465052;  // "RPF1"
constexpr size_t kRecordBytes = 38;
constexpr uint32_t kMaxRaw = 4 * 1024 * 1024;

void put32(std::string* s, uint32_t v) {
  for (int i = 0; i < 4; ++i) s->push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
void put16(std::string* s, uint16_t v) {
  s->push_back(static_cast<char>(v & 0xFF));
  s->push_back(static_cast<char>(v >> 8));
}
uint32_t get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
void putStr(std::string* s, const char* v, size_t n) {
  for (size_t i = 0; i < n; ++i) s->push_back(i < std::strlen(v) ? v[i] : '\0');
}
void getStr(const uint8_t* p, size_t n, char* out, size_t out_size) {
  size_t i = 0;
  for (; i < n && i + 1 < out_size && p[i] != 0; ++i) out[i] = static_cast<char>(p[i]);
  out[i] = '\0';
}

}  // namespace

std::string encodeFrame(int64_t time_s, const std::vector<Aircraft>& aircraft) {
  // Per aircraft: hex(4) lat(4) lon(4) alt(4) track(2) heading(2) gs(2) vrate(2) flags(1)
  //               callsign(8) type(4) squawk(1, BCD-ish: skipped) -> 38 bytes with padding.
  std::string raw;
  raw.reserve(aircraft.size() * kRecordBytes);
  for (const Aircraft& a : aircraft) {
    char* end = nullptr;
    const unsigned long hex = std::strtoul(a.hex[0] == '~' ? a.hex + 1 : a.hex, &end, 16);
    put32(&raw, static_cast<uint32_t>(hex) | (a.hex[0] == '~' ? 0x80000000u : 0));
    put32(&raw, static_cast<uint32_t>(static_cast<int32_t>(std::lround(a.lat * 1e6))));
    put32(&raw, static_cast<uint32_t>(static_cast<int32_t>(std::lround(a.lon * 1e6))));
    put32(&raw, static_cast<uint32_t>(a.alt_ft));
    put16(&raw, static_cast<uint16_t>(std::lround(std::fmod(a.track_deg + 360.0f, 360.0f) * 100)));
    put16(&raw, static_cast<uint16_t>(std::lround(std::fmod(a.heading_deg + 360.0f, 360.0f) * 100)));
    put16(&raw, static_cast<uint16_t>(std::lround(std::max(0.0f, std::min(a.gs_kt, 6000.0f)) * 10)));
    put16(&raw, static_cast<uint16_t>(static_cast<int16_t>(std::max(-32000, std::min(32000, static_cast<int>(a.vrate_fpm))))));
    raw.push_back(static_cast<char>((a.alt_valid ? 1 : 0) | (a.on_ground ? 2 : 0) | (a.emergency ? 4 : 0) |
                                    (a.military ? 8 : 0)));
    putStr(&raw, a.callsign, 8);
    putStr(&raw, a.type, 4);
    raw.push_back(0);  // spare
  }
  const std::string packed = deflateRaw(reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
  std::string out;
  put32(&out, kMagic);
  put32(&out, static_cast<uint32_t>(time_s));
  put32(&out, static_cast<uint32_t>(packed.size()));
  put32(&out, static_cast<uint32_t>(raw.size()));
  return out + packed;
}

bool readHeader(const uint8_t* p, FrameHeader* out) {
  if (get32(p) != kMagic) return false;
  out->time_s = get32(p + 4);
  out->packed = get32(p + 8);
  out->raw = get32(p + 12);
  return out->raw <= kMaxRaw && out->raw % kRecordBytes == 0 && out->packed <= kMaxRaw + 1024;
}

bool decodeFrame(const FrameHeader& h, const uint8_t* packed, std::vector<Aircraft>* out) {
  out->clear();
  std::string raw;
  if (!inflateRaw(packed, h.packed, &raw, h.raw) || raw.size() != h.raw) return false;
  const auto* p = reinterpret_cast<const uint8_t*>(raw.data());
  for (size_t off = 0; off + kRecordBytes <= raw.size(); off += kRecordBytes) {
    const uint8_t* r = p + off;
    Aircraft a;
    const uint32_t hex = get32(r);
    std::snprintf(a.hex, sizeof(a.hex), (hex & 0x80000000u) ? "~%06x" : "%06x", static_cast<unsigned>(hex & 0xFFFFFF));
    a.lat = static_cast<int32_t>(get32(r + 4)) / 1e6;
    a.lon = static_cast<int32_t>(get32(r + 8)) / 1e6;
    a.alt_ft = static_cast<int32_t>(get32(r + 12));
    a.track_deg = get16(r + 16) / 100.0f;
    a.heading_deg = get16(r + 18) / 100.0f;
    a.gs_kt = get16(r + 20) / 10.0f;
    a.vrate_fpm = static_cast<int16_t>(get16(r + 22));
    const uint8_t f = r[24];
    a.alt_valid = f & 1;
    a.on_ground = f & 2;
    a.emergency = f & 4;
    a.military = f & 8;
    getStr(r + 25, 8, a.callsign, sizeof(a.callsign));
    getStr(r + 33, 4, a.type, sizeof(a.type));
    out->push_back(a);
  }
  return true;
}

std::string fileName(int date) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "replay/%04d-%02d-%02d.rpl", date / 10000, date / 100 % 100, date % 100);
  return buf;
}

int dateOfFile(const std::string& name) {
  int y, m, d;
  char tail[8] = "";
  if (name.size() != 14 || std::sscanf(name.c_str(), "%4d-%2d-%2d.%3s", &y, &m, &d, tail) != 4 ||
      std::strcmp(tail, "rpl") != 0 || m < 1 || m > 12 || d < 1 || d > 31) {
    return 0;
  }
  return y * 10000 + m * 100 + d;
}

}  // namespace core::replay
