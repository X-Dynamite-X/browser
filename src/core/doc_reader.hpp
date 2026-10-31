#pragma once

#include "docx_reader.hpp"
#include <string>

namespace litebrowser {

// Parses legacy Microsoft Word 97-2003 (.doc OLE2) and Rich Text Format (.rtf) files.
DocxParseResult parseLegacyDocOrRtfFile(const std::string& filePathUtf8);

}  // namespace litebrowser

