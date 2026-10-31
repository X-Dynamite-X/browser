#pragma once

#include <string>

namespace litebrowser {

struct DocumentViewOutput {
    bool ok = false;
    std::string tabTitle;
    std::string fullHtml;
    std::string tempHtmlPath;
    std::string tempFileUrl;
};

bool isSupportedDocumentFile(const std::string& pathOrUrl);

// Renders .pdf, .docx, .doc, .rtf, .txt, .md into a standalone HTML reader view
// and writes a session temp file so any engine (WebKit2GTK, Miniblink, WebView2, WinXP MSHTML)
// can load even multi-megabyte documents with zero URL-length limits.
DocumentViewOutput renderDocumentToHtml(const std::string& filePathUtf8, int slotId = 0);

}  // namespace litebrowser

