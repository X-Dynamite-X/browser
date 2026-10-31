#pragma once

#include <string>
#include <vector>

namespace litebrowser {

struct PdfPageContent {
    int pageNumber = 1;
    std::string imageFileUrl; // file:// url pointing to lazy-loaded page image
    std::string pngBase64;   // Populated when high-fidelity visual rendering is available
    std::string textUtf8;    // Extracted searchable text for this page
    std::vector<std::string> jpegImagesBase64;
};

struct PdfParseResult {
    bool ok = false;
    bool isEncrypted = false;
    std::string title;
    int totalPages = 0;
    bool hasRenderedImages = false;
    std::vector<PdfPageContent> pages;
    std::vector<std::string> outlines;
    std::string errorMessage;
};

PdfParseResult parsePdfFile(const std::string& filePathUtf8, int maxVisualPages = 60);
PdfParseResult parsePdfPortable(const std::string& filePathUtf8);

}  // namespace litebrowser
