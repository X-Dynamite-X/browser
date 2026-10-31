#pragma once

#include <string>

namespace litebrowser {

enum class OmniboxActionType {
    NavigateUrl,
    OpenLocalDocument,
    InternalPage
};

struct ResolvedInput {
    OmniboxActionType type = OmniboxActionType::NavigateUrl;
    std::string targetUrlOrPath;
};

ResolvedInput resolveOmniboxInput(const std::string& rawInput, bool liteMode, const std::string& searchEngine = "google");

bool shouldBlockAdOrTracker(const std::string& requestUrl);
bool isDirectDownloadExtension(const std::string& urlOrPath);
std::string extractHostLower(const std::string& url);

}  // namespace litebrowser

