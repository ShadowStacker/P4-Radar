#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace core {

/** True if the data starts with the gzip magic bytes (1f 8b). */
bool isGzip(const void* data, size_t len);

/**
 * Decompresses a gzip stream into `out`. Stops with false on corrupt input or
 * once the output would exceed `max_out` bytes. The CRC is not checked: TLS
 * already guarantees the bytes arrived intact.
 */
bool gunzip(const void* data, size_t len, std::string* out, size_t max_out);

/** Decompresses a raw DEFLATE stream (RFC 1951). Same rules as gunzip(). */
bool inflateRaw(const void* data, size_t len, std::string* out, size_t max_out);

}  // namespace core
