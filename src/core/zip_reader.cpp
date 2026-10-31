#include "zip_reader.hpp"

#include "deflate.hpp"
#include "fs_utils.hpp"

#include <algorithm>
#include <cstring>

namespace litebrowser {

namespace {

uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

std::string normalizeZipName(const std::string& raw) {
    std::string out = raw;
    std::replace(out.begin(), out.end(), '\\', '/');
    while (!out.empty() && out.front() == '/') out.erase(out.begin());
    return out;
}

}  // namespace

bool ZipArchive::openFromFile(const std::string& filePathUtf8) {
    std::vector<uint8_t> bytes;
    if (!fs::readFileBytes(filePathUtf8, bytes)) return false;
    return openFromMemory(std::move(bytes));
}

bool ZipArchive::openFromMemory(std::vector<uint8_t> data) {
    entries_.clear();
    data_ = std::move(data);
    if (data_.size() < 22) return false;

    // Locate End of Central Directory (0x06054b50) searching backwards up to 65557 bytes
    size_t minOff = (data_.size() > 65557) ? (data_.size() - 65557) : 0;
    size_t eocdPos = std::string::npos;
    for (size_t pos = data_.size() - 22; ; --pos) {
        if (readU32LE(data_.data() + pos) == 0x06054b50u) {
            eocdPos = pos;
            break;
        }
        if (pos == minOff) break;
    }
    if (eocdPos == std::string::npos) return false;

    uint16_t totalEntries = readU16LE(data_.data() + eocdPos + 10);
    uint32_t cdSize = readU32LE(data_.data() + eocdPos + 12);
    uint32_t cdOffset = readU32LE(data_.data() + eocdPos + 16);
    if (static_cast<uint64_t>(cdOffset) + cdSize > data_.size()) return false;

    size_t cur = cdOffset;
    size_t cdEnd = cdOffset + cdSize;
    for (uint16_t i = 0; i < totalEntries && cur + 46 <= cdEnd; ++i) {
        if (readU32LE(data_.data() + cur) != 0x02014b50u) break;
        uint16_t method = readU16LE(data_.data() + cur + 10);
        uint32_t compSz = readU32LE(data_.data() + cur + 20);
        uint32_t uncompSz = readU32LE(data_.data() + cur + 24);
        uint16_t nameLen = readU16LE(data_.data() + cur + 28);
        uint16_t extraLen = readU16LE(data_.data() + cur + 30);
        uint16_t commentLen = readU16LE(data_.data() + cur + 32);
        uint32_t localOff = readU32LE(data_.data() + cur + 42);

        if (cur + 46 + nameLen > cdEnd) break;
        std::string rawName(reinterpret_cast<const char*>(data_.data() + cur + 46), nameLen);
        std::string normName = normalizeZipName(rawName);

        ZipEntryInfo info;
        info.name = normName;
        info.method = method;
        info.compressedSize = compSz;
        info.uncompressedSize = uncompSz;
        info.localHeaderOffset = localOff;
        entries_[normName] = info;

        cur += 46 + static_cast<size_t>(nameLen) + extraLen + commentLen;
    }

    return !entries_.empty();
}

bool ZipArchive::hasEntry(const std::string& entryPath) const {
    return entries_.find(normalizeZipName(entryPath)) != entries_.end();
}

bool ZipArchive::extractEntry(const std::string& entryPath, std::vector<uint8_t>& out) const {
    out.clear();
    auto it = entries_.find(normalizeZipName(entryPath));
    if (it == entries_.end()) return false;

    const ZipEntryInfo& info = it->second;
    size_t lh = info.localHeaderOffset;
    if (lh + 30 > data_.size()) return false;
    if (readU32LE(data_.data() + lh) != 0x04034b50u) return false;

    uint16_t nameLen = readU16LE(data_.data() + lh + 26);
    uint16_t extraLen = readU16LE(data_.data() + lh + 28);
    size_t dataOffset = lh + 30 + static_cast<size_t>(nameLen) + extraLen;
    if (dataOffset + info.compressedSize > data_.size()) return false;

    if (info.method == 0) {
        out.assign(data_.begin() + static_cast<std::ptrdiff_t>(dataOffset),
                   data_.begin() + static_cast<std::ptrdiff_t>(dataOffset + info.compressedSize));
        return true;
    } else if (info.method == 8) {
        if (info.uncompressedSize == 0 && info.compressedSize <= 2) {
            return true;
        }
        return compression::inflateRaw(data_.data() + dataOffset, info.compressedSize, out);
    }
    return false;
}

bool ZipArchive::extractEntryText(const std::string& entryPath, std::string& out) const {
    std::vector<uint8_t> bytes;
    if (!extractEntry(entryPath, bytes)) return false;
    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

std::vector<std::string> ZipArchive::listEntries() const {
    std::vector<std::string> names;
    names.reserve(entries_.size());
    for (const auto& kv : entries_) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace litebrowser

