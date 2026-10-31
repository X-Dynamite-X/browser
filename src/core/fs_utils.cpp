#include "fs_utils.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace litebrowser {
namespace fs {

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &out[0], len);
    return out;
}

std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return std::string();
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), &out[0], len, nullptr, nullptr);
    return out;
}
#endif

bool readFileBytes(const std::string& pathUtf8, std::vector<uint8_t>& out) {
    out.clear();
#ifdef _WIN32
    std::wstring wpath = utf8ToWide(pathUtf8);
    FILE* f = _wfopen(wpath.c_str(), L"rb");
#else
    FILE* f = std::fopen(pathUtf8.c_str(), "rb");
#endif
    if (!f) return false;
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return false;
    }
    long sz = std::ftell(f);
    if (sz < 0) {
        std::fclose(f);
        return false;
    }
    std::fseek(f, 0, SEEK_SET);
    out.resize(static_cast<size_t>(sz));
    if (sz > 0) {
        size_t n = std::fread(out.data(), 1, static_cast<size_t>(sz), f);
        out.resize(n);
    }
    std::fclose(f);
    return true;
}

bool readFileText(const std::string& pathUtf8, std::string& out) {
    std::vector<uint8_t> bytes;
    if (!readFileBytes(pathUtf8, bytes)) return false;
    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

bool writeFileBytes(const std::string& pathUtf8, const uint8_t* data, size_t size) {
#ifdef _WIN32
    std::wstring wpath = utf8ToWide(pathUtf8);
    FILE* f = _wfopen(wpath.c_str(), L"wb");
#else
    FILE* f = std::fopen(pathUtf8.c_str(), "wb");
#endif
    if (!f) return false;
    size_t written = (size > 0 && data) ? std::fwrite(data, 1, size, f) : 0;
    std::fclose(f);
    return written == size;
}

bool writeFileText(const std::string& pathUtf8, const std::string& content) {
    return writeFileBytes(pathUtf8, reinterpret_cast<const uint8_t*>(content.data()), content.size());
}

bool fileExists(const std::string& pathUtf8) {
    if (pathUtf8.empty()) return false;
#ifdef _WIN32
    std::wstring wpath = utf8ToWide(pathUtf8);
    DWORD attr = GetFileAttributesW(wpath.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    if (stat(pathUtf8.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
#endif
}

uint64_t fileSize(const std::string& pathUtf8) {
#ifdef _WIN32
    std::wstring wpath = utf8ToWide(pathUtf8);
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &fad)) return 0;
    return (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
#else
    struct stat st;
    if (stat(pathUtf8.c_str(), &st) != 0) return 0;
    return static_cast<uint64_t>(st.st_size);
#endif
}

static bool makeSingleDir(const std::string& p) {
    if (p.empty()) return true;
#ifdef _WIN32
    std::wstring wp = utf8ToWide(p);
    DWORD attr = GetFileAttributesW(wp.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return true;
    return CreateDirectoryW(wp.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    struct stat st;
    if (stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
    return mkdir(p.c_str(), 0755) == 0;
#endif
}

bool ensureDir(const std::string& dirUtf8) {
    if (dirUtf8.empty()) return false;
    std::string norm = dirUtf8;
#ifdef _WIN32
    std::replace(norm.begin(), norm.end(), '/', '\\');
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    std::string cur;
    for (size_t i = 0; i < norm.size(); ++i) {
        cur.push_back(norm[i]);
        if (norm[i] == sep && cur.size() > 1) {
#ifdef _WIN32
            if (cur.size() == 3 && cur[1] == ':') continue;
#endif
            makeSingleDir(cur);
        }
    }
    return makeSingleDir(cur);
}

void removeFile(const std::string& pathUtf8) {
    if (pathUtf8.empty()) return;
#ifdef _WIN32
    std::wstring wp = utf8ToWide(pathUtf8);
    DeleteFileW(wp.c_str());
#else
    unlink(pathUtf8.c_str());
#endif
}

std::string toLowerAscii(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
    size_t e = s.size();
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
    return s.substr(b, e - b);
}

bool startsWithIgnoreCase(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        char a = s[i];
        char b = prefix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

std::string getFileName(const std::string& pathOrUrl) {
    std::string clean = pathOrUrl;
    size_t q = clean.find_first_of("?#");
    if (q != std::string::npos) clean.erase(q);
    while (!clean.empty() && (clean.back() == '/' || clean.back() == '\\')) clean.pop_back();
    size_t slash = clean.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? clean : clean.substr(slash + 1);
    return urlDecode(name);
}

std::string getExtensionLower(const std::string& pathOrUrl) {
    std::string name = getFileName(pathOrUrl);
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return "";
    return toLowerAscii(name.substr(dot));
}

std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    if (dir.back() == '/' || dir.back() == '\\') return dir + name;
    return dir + sep + name;
}

std::string pathToFileUrl(const std::string& pathUtf8) {
    std::string p = pathUtf8;
    std::replace(p.begin(), p.end(), '\\', '/');
    std::string encoded;
    for (unsigned char c : p) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '/' || c == '-' || c == '_' || c == '.' || c == ':' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            encoded += buf;
        }
    }
    if (!encoded.empty() && encoded[0] == '/') {
        return "file://" + encoded;
    }
    return "file:///" + encoded;
}

std::string fileUrlToPath(const std::string& url) {
    if (!startsWithIgnoreCase(url, "file://")) return url;
    std::string rest = url.substr(7);
#ifdef _WIN32
    if (rest.size() >= 3 && rest[0] == '/' && rest[2] == ':') {
        rest.erase(0, 1);
    }
    std::string decoded = urlDecode(rest);
    std::replace(decoded.begin(), decoded.end(), '/', '\\');
    return decoded;
#else
    return urlDecode(rest);
#endif
}

std::string getConfigDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf))) {
        std::string dir = joinPath(wideToUtf8(buf), "LiteBrowser");
        ensureDir(dir);
        return dir;
    }
    return ".";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::string base;
    if (xdg && *xdg) {
        base = xdg;
    } else {
        const char* home = std::getenv("HOME");
        base = home ? joinPath(home, ".config") : "/tmp";
    }
    std::string dir = joinPath(base, "litebrowser");
    ensureDir(dir);
    return dir;
#endif
}

std::string getDownloadsDir() {
#ifdef _WIN32
    // Compatible with Windows XP (My Documents\Downloads) AND Windows 7/8/10/11 (USERPROFILE\Downloads)
    const wchar_t* userProf = _wgetenv(L"USERPROFILE");
    if (userProf && *userProf) {
        std::string dl = joinPath(wideToUtf8(userProf), "Downloads");
        DWORD attr = GetFileAttributesW(utf8ToWide(dl).c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return dl;
        }
    }
    wchar_t docs[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, docs))) {
        std::string dl = joinPath(wideToUtf8(docs), "Downloads");
        ensureDir(dl);
        return dl;
    }
    return ".";
#else
    const char* home = std::getenv("HOME");
    if (home && *home) {
        std::string dl = joinPath(home, "Downloads");
        ensureDir(dl);
        return dl;
    }
    std::string fallback = "/tmp/litebrowser-downloads";
    ensureDir(fallback);
    return fallback;
#endif
}

static std::string g_sessionDir;

std::string getTempSessionDir() {
    if (!g_sessionDir.empty()) return g_sessionDir;
#ifdef _WIN32
    wchar_t tmp[MAX_PATH] = {0};
    DWORD n = GetTempPathW(MAX_PATH, tmp);
    std::string base = (n > 0) ? wideToUtf8(tmp) : ".";
    char pidBuf[64];
    std::snprintf(pidBuf, sizeof(pidBuf), "litebrowser_session_%lu", static_cast<unsigned long>(GetCurrentProcessId()));
    g_sessionDir = joinPath(base, pidBuf);
#else
    char pidBuf[64];
    std::snprintf(pidBuf, sizeof(pidBuf), "/tmp/litebrowser_session_%d", static_cast<int>(getpid()));
    g_sessionDir = pidBuf;
#endif
    ensureDir(g_sessionDir);
    return g_sessionDir;
}

void cleanupTempSessionDir() {
    if (g_sessionDir.empty()) return;
#ifdef _WIN32
    std::wstring pattern = utf8ToWide(joinPath(g_sessionDir, "*"));
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                std::wstring fp = utf8ToWide(g_sessionDir) + L"\\" + fd.cFileName;
                DeleteFileW(fp.c_str());
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(utf8ToWide(g_sessionDir).c_str());
#else
    DIR* d = opendir(g_sessionDir.c_str());
    if (d) {
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (std::strcmp(ent->d_name, ".") != 0 && std::strcmp(ent->d_name, "..") != 0) {
                std::string fp = joinPath(g_sessionDir, ent->d_name);
                unlink(fp.c_str());
            }
        }
        closedir(d);
    }
    rmdir(g_sessionDir.c_str());
#endif
    g_sessionDir.clear();
}

std::string base64Encode(const uint8_t* data, size_t len) {
    static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) v |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) v |= static_cast<uint32_t>(data[i + 2]);
        out.push_back(kTable[(v >> 18) & 0x3F]);
        out.push_back(kTable[(v >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kTable[(v >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? kTable[v & 0x3F] : '=');
    }
    return out;
}

std::string htmlEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + text.size() / 8);
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string urlEncode(const std::string& text) {
    std::string out;
    out.reserve(text.size() * 2);
    for (unsigned char c : text) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            int hi = hexVal(text[i + 1]);
            int lo = hexVal(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        } else if (text[i] == '+') {
            out.push_back(' ');
            continue;
        }
        out.push_back(text[i]);
    }
    return out;
}

bool containsArabicText(const std::string& utf8Text) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(utf8Text.data());
    size_t n = utf8Text.size();
    for (size_t i = 0; i + 1 < n; ++i) {
        // UTF-8 range U+0600..U+06FF is 0xD8 0x80 .. 0xDB 0xBF
        if (bytes[i] >= 0xD8 && bytes[i] <= 0xDB && (bytes[i + 1] & 0xC0) == 0x80) {
            return true;
        }
    }
    return false;
}

std::string formatBytes(uint64_t bytes) {
    char buf[64];
    if (bytes < 1024) {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    } else if (bytes < 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    } else if (bytes < 1024ULL * 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

}  // namespace fs
}  // namespace litebrowser

