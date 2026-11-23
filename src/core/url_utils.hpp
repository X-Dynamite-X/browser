#pragma once

#include <string>

namespace litebrowser {

enum class OmniboxActionType {
    NavigateUrl,
    OpenLocalDocument,
    InternalPage
};

enum class SecurityLevel {
    InternalSafe,
    SecureHttps,
    InsecureHttp
};

struct ResolvedInput {
    OmniboxActionType type = OmniboxActionType::NavigateUrl;
    std::string targetUrlOrPath;
};

ResolvedInput resolveOmniboxInput(const std::string& rawInput, bool liteMode, const std::string& searchEngine = "google");

void loadCustomAdblockRules(const std::string& filePathUtf8);
bool shouldBlockAdOrTracker(const std::string& requestUrl);
bool isDirectDownloadExtension(const std::string& urlOrPath);
bool isRemoteDocumentUrl(const std::string& url);
SecurityLevel getUrlSecurityLevel(const std::string& url);
std::string extractHostLower(const std::string& url);

}  // namespace litebrowser
