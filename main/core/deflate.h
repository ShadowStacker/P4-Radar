#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace core {

/**
 * Compresses with DEFLATE (RFC 1951) using the fixed Huffman codes and a
 * short-chain LZ77 search: fast and small, and plenty for screenshots, which
 * are mostly long runs of the same color.
 */
std::string deflateRaw(const uint8_t* data, size_t len);
/** The same, wrapped as a zlib stream (RFC 1950), as PNG wants it. */
std::string zlibCompress(const uint8_t* data, size_t len);

uint32_t crc32(uint32_t crc, const void* data, size_t len);
uint32_t adler32(uint32_t adler, const uint8_t* data, size_t len);

}  // namespace core
