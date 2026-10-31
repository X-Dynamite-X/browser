#pragma once

#include <string>

namespace litebrowser {

struct DocxParseResult {
    bool ok = false;
    std::string title;
    std::string bodyHtml;
    std::string plainText;
    size_t paragraphCount = 0;
    size_t wordCount = 0;
    std::string errorMessage;
};

DocxParseResult parseDocxFile(const std::string& filePathUtf8);

}  // namespace litebrowser

