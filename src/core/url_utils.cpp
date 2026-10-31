#include "url_utils.hpp"

#include "document_viewer.hpp"
#include "fs_utils.hpp"

#include <cctype>
#include <vector>

namespace litebrowser {

namespace {

bool looksLikeWindowsPath(const std::string& s) {
    return s.size() >= 3 && std::isalpha(static_cast<unsigned char>(s[0])) &&
           s[1] == ':' && (s[2] == '\\' || s[2] == '/');
}

bool looksLikeDomainOrLocalhost(const std::string& s) {
    if (s.find(' ') != std::string::npos || s.find('\t') != std::string::npos) return false;
    std::string host = s;
    size_t slash = host.find('/');
    if (slash != std::string::npos) host = host.substr(0, slash);
    size_t colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0, colon);

    if (fs::toLowerAscii(host) == "localhost") return true;
    size_t dot = host.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= host.size()) return false;
    return true;
}

}  // namespace

std::string extractHostLower(const std::string& url) {
    size_t p = url.find("://");
    size_t start = (p == std::string::npos) ? 0 : (p + 3);
    size_t end = url.find_first_of("/:?#", start);
    std::string host = (end == std::string::npos) ? url.substr(start) : url.substr(start, end - start);
    return fs::toLowerAscii(host);
}

ResolvedInput resolveOmniboxInput(const std::string& rawInput, bool liteMode, const std::string& searchEngine) {
    ResolvedInput res;
    std::string input = fs::trim(rawInput);

    if (input.empty() || fs::toLowerAscii(input) == "lite://home" || fs::toLowerAscii(input) == "about:home") {
        res.type = OmniboxActionType::InternalPage;
        res.targetUrlOrPath = "lite://home";
        return res;
    }

    if (fs::startsWithIgnoreCase(input, "lite://")) {
        res.type = OmniboxActionType::InternalPage;
        res.targetUrlOrPath = fs::toLowerAscii(input);
        return res;
    }

    // Check file:// URLs
    if (fs::startsWithIgnoreCase(input, "file://")) {
        std::string localPath = fs::fileUrlToPath(input);
        if (isSupportedDocumentFile(localPath) && fs::fileExists(localPath)) {
            res.type = OmniboxActionType::OpenLocalDocument;
            res.targetUrlOrPath = localPath;
            return res;
        }
        res.type = OmniboxActionType::NavigateUrl;
        res.targetUrlOrPath = input;
        return res;
    }

    // Check direct local filesystem path (/path/... on Linux or C:\... on Windows)
    if ((!input.empty() && input[0] == '/') || looksLikeWindowsPath(input)) {
        if (isSupportedDocumentFile(input)) {
            res.type = OmniboxActionType::OpenLocalDocument;
            res.targetUrlOrPath = input;
            return res;
        }
        if (fs::fileExists(input)) {
            res.type = OmniboxActionType::NavigateUrl;
            res.targetUrlOrPath = fs::pathToFileUrl(input);
            return res;
        }
    }

    // Explicit http:// or https://
    if (fs::startsWithIgnoreCase(input, "http://") || fs::startsWithIgnoreCase(input, "https://")) {
        res.type = OmniboxActionType::NavigateUrl;
        res.targetUrlOrPath = input;
        return res;
    }

    // Domain or localhost without scheme
    if (looksLikeDomainOrLocalhost(input)) {
        res.type = OmniboxActionType::NavigateUrl;
        std::string lower = fs::toLowerAscii(input);
        if (fs::startsWithIgnoreCase(lower, "localhost") || fs::startsWithIgnoreCase(lower, "127.") ||
            fs::startsWithIgnoreCase(lower, "192.168.") || fs::startsWithIgnoreCase(lower, "10.")) {
            res.targetUrlOrPath = "http://" + input;
        } else {
            res.targetUrlOrPath = "https://" + input;
        }
        return res;
    }

    // Otherwise treat as search query
    res.type = OmniboxActionType::NavigateUrl;
    std::string encoded = fs::urlEncode(input);
    if (liteMode || searchEngine == "ddg-lite") {
        res.targetUrlOrPath = "https://html.duckduckgo.com/html/?q=" + encoded;
    } else if (searchEngine == "ddg") {
        res.targetUrlOrPath = "https://duckduckgo.com/?q=" + encoded;
    } else if (searchEngine == "youtube") {
        res.targetUrlOrPath = "https://www.youtube.com/results?search_query=" + encoded;
    } else {
        res.targetUrlOrPath = "https://www.google.com/search?q=" + encoded;
    }
    return res;
}

bool shouldBlockAdOrTracker(const std::string& requestUrl) {
    static const char* kBlockedDomains[] = {
        "doubleclick.net",
        "googlesyndication.com",
        "googleadservices.com",
        "adnxs.com",
        "taboola.com",
        "outbrain.com",
        "popads.net",
        "popcash.net",
        "criteo.com",
        "criteo.net",
        "scorecardresearch.com",
        "rubiconproject.com",
        "pubmatic.com",
        "openx.net",
        "advertising.com",
        "moatads.com",
        "adform.net",
        "yandex.ru/metrika",
        "mc.yandex.ru"
    };

    std::string host = extractHostLower(requestUrl);
    if (host.empty()) return false;

    for (const char* dom : kBlockedDomains) {
        std::string d(dom);
        if (host == d) return true;
        if (host.size() > d.size() &&
            host.compare(host.size() - d.size(), d.size(), d) == 0 &&
            host[host.size() - d.size() - 1] == '.') {
            return true;
        }
    }
    return false;
}

bool isDirectDownloadExtension(const std::string& urlOrPath) {
    std::string ext = fs::getExtensionLower(urlOrPath);
    return (ext == ".exe" || ext == ".msi" || ext == ".zip" || ext == ".rar" ||
            ext == ".7z" || ext == ".tar" || ext == ".gz" || ext == ".bz2" ||
            ext == ".xz" || ext == ".deb" || ext == ".rpm" || ext == ".appimage" ||
            ext == ".iso" || ext == ".apk" || ext == ".dmg");
}

}  // namespace litebrowser

