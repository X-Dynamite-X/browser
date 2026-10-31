#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace litebrowser {
namespace compression {

// Decompresses raw RFC-1951 DEFLATE bitstream (used inside ZIP/.docx archives).
bool inflateRaw(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t maxOutputBytes = 128 * 1024 * 1024);

// Decompresses RFC-1950 zlib-wrapped DEFLATE stream (used inside PDF FlateDecode streams),
// automatically falling back to raw DEFLATE if header does not match zlib.
bool inflateZlibOrRaw(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t maxOutputBytes = 128 * 1024 * 1024);

}  // namespace compression
}  // namespace litebrowser

