#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace litebrowser {

struct ZipEntryInfo {
    std::string name;
    uint16_t method = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint32_t localHeaderOffset = 0;
};

class ZipArchive {
public:
    bool openFromMemory(std::vector<uint8_t> data);
    bool openFromFile(const std::string& filePathUtf8);

    bool hasEntry(const std::string& entryPath) const;
    bool extractEntry(const std::string& entryPath, std::vector<uint8_t>& out) const;
    bool extractEntryText(const std::string& entryPath, std::string& out) const;
    std::vector<std::string> listEntries() const;

private:
    std::vector<uint8_t> data_;
    std::unordered_map<std::string, ZipEntryInfo> entries_;
};

}  // namespace litebrowser

