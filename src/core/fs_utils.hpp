#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace litebrowser {
namespace fs {

// File I/O (UTF-8 paths on both Linux and Windows XP+)
bool readFileBytes(const std::string& pathUtf8, std::vector<uint8_t>& out);
bool readFileText(const std::string& pathUtf8, std::string& out);
bool writeFileBytes(const std::string& pathUtf8, const uint8_t* data, size_t size);
bool writeFileText(const std::string& pathUtf8, const std::string& content);

bool fileExists(const std::string& pathUtf8);
uint64_t fileSize(const std::string& pathUtf8);
bool ensureDir(const std::string& dirUtf8);
void removeFile(const std::string& pathUtf8);

// Path helpers
std::string getExtensionLower(const std::string& pathOrUrl);
std::string getFileName(const std::string& pathOrUrl);
std::string joinPath(const std::string& dir, const std::string& name);
std::string pathToFileUrl(const std::string& pathUtf8);
std::string fileUrlToPath(const std::string& url);

// Standard directories
std::string getConfigDir();
std::string getDownloadsDir();
std::string getTempSessionDir();
void cleanupTempSessionDir();

// Encoding & string helpers
std::string base64Encode(const uint8_t* data, size_t len);
std::string htmlEscape(const std::string& text);
std::string urlEncode(const std::string& text);
std::string urlDecode(const std::string& text);
std::string toLowerAscii(const std::string& s);
std::string trim(const std::string& s);
bool startsWithIgnoreCase(const std::string& s, const std::string& prefix);
bool containsArabicText(const std::string& utf8Text);
std::string formatBytes(uint64_t bytes);

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& utf8);
std::string wideToUtf8(const std::wstring& wide);
#endif

}  // namespace fs
}  // namespace litebrowser

