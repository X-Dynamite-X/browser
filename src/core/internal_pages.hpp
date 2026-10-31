#pragma once

#include "storage.hpp"
#include <string>

namespace litebrowser {

struct InternalPageOutput {
    std::string title;
    std::string html;
    std::string tempHtmlPath;
    std::string tempFileUrl;
};

InternalPageOutput renderInternalPage(const std::string& liteUri, StorageManager& storage, int slotId = 0);

}  // namespace litebrowser

