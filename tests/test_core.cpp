#include "core/deflate.hpp"
#include "core/doc_reader.hpp"
#include "core/document_viewer.hpp"
#include "core/docx_reader.hpp"
#include "core/fs_utils.hpp"
#include "core/internal_pages.hpp"
#include "core/pdf_reader.hpp"
#include "core/storage.hpp"
#include "core/url_utils.hpp"
#include "core/zip_reader.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool cond, const char* msg) {
    if (cond) {
        ++g_passed;
        std::printf("  [PASS] %s\n", msg);
    } else {
        ++g_failed;
        std::fprintf(stderr, "  [FAIL] %s\n", msg);
    }
}

void testOmniboxAndAdblock() {
    std::printf("\n--- Testing Omnibox & AdBlock ---\n");
    auto r1 = litebrowser::resolveOmniboxInput("youtube.com", false);
    check(r1.type == litebrowser::OmniboxActionType::NavigateUrl && r1.targetUrlOrPath == "https://youtube.com",
          "Domain auto-expands to https://youtube.com");

    auto r2 = litebrowser::resolveOmniboxInput("العاب خفيفة", false, "google");
    check(r2.targetUrlOrPath.find("https://www.google.com/search?q=") == 0,
          "Arabic search query routes to Google in standard mode");

    auto r3 = litebrowser::resolveOmniboxInput("العاب خفيفة", true, "google");
    check(r3.targetUrlOrPath.find("https://html.duckduckgo.com/html/?q=") == 0,
          "Lite mode routes search to lightweight HTML DuckDuckGo");

    auto r4 = litebrowser::resolveOmniboxInput("lite://games", false);
    check(r4.type == litebrowser::OmniboxActionType::InternalPage && r4.targetUrlOrPath == "lite://games",
          "Internal lite://games page detected");

    check(litebrowser::shouldBlockAdOrTracker("https://ads.doubleclick.net/pagead/script.js"),
          "AdBlocker blocks doubleclick.net");
    check(!litebrowser::shouldBlockAdOrTracker("https://www.youtube.com/watch?v=abc"),
          "AdBlocker allows youtube.com");
    check(litebrowser::isDirectDownloadExtension("https://example.org/files/installer.exe"),
          "Detects .exe program installer download");
}

void testDocuments(const std::string& samplesDir) {
    std::printf("\n--- Testing DOCX, Legacy DOC (OLE2), RTF, and PDF Readers ---\n");

    // 1. DOCX
    std::string docxPath = litebrowser::fs::joinPath(samplesDir, "arabic_guide.docx");
    auto docx = litebrowser::parseDocxFile(docxPath);
    check(docx.ok, "DOCX file parsed successfully");
    check(docx.title.find("دليل") != std::string::npos, "DOCX metadata title extracted in Arabic");
    check(docx.bodyHtml.find("مرحباً بك في القارئ المدمج") != std::string::npos, "DOCX Arabic heading extracted");
    check(docx.bodyHtml.find("<table class=\"doc-table\">") != std::string::npos, "DOCX table rendered to HTML");
    check(docx.bodyHtml.find("data:image/png;base64,") != std::string::npos, "DOCX embedded PNG image extracted as Base64");
    check(docx.bodyHtml.find("https://ar.wikipedia.org") != std::string::npos, "DOCX external hyperlink preserved");

    // 2. Legacy OLE2 .doc
    std::string docPath = litebrowser::fs::joinPath(samplesDir, "legacy_xp_note.doc");
    auto doc = litebrowser::parseLegacyDocOrRtfFile(docPath);
    check(doc.ok, "Legacy OLE2 .doc parsed successfully");
    check(doc.plainText.find("Windows XP") != std::string::npos &&
          doc.plainText.find("العربية") != std::string::npos,
          "Legacy OLE2 .doc extracted both Arabic and English UTF-16LE piece table text");

    // 3. RTF
    std::string rtfPath = litebrowser::fs::joinPath(samplesDir, "sample_article.rtf");
    auto rtf = litebrowser::parseLegacyDocOrRtfFile(rtfPath);
    check(rtf.ok && rtf.plainText.find("مرحباً") != std::string::npos &&
          rtf.plainText.find("Sample RTF") != std::string::npos,
          "RTF document Unicode escapes and paragraphs extracted");

    // 4. PDF (both Portable C++17 engine and Linux Poppler+Cairo visual engine)
    std::string pdfPath = litebrowser::fs::joinPath(samplesDir, "sample_handbook.pdf");
    auto pdfPortable = litebrowser::parsePdfPortable(pdfPath);
    check(pdfPortable.ok && pdfPortable.totalPages == 2, "Portable C++17 PDF parser detected 2 pages");
    check(!pdfPortable.pages.empty() &&
          pdfPortable.pages[0].textUtf8.find("LiteBrowser Built-in PDF Reader") != std::string::npos,
          "Portable C++17 PDF parser decompressed FlateDecode stream & extracted Page 1 text");
    check(pdfPortable.pages.size() >= 2 &&
          pdfPortable.pages[1].textUtf8.find("العربية") != std::string::npos,
          "Portable C++17 PDF parser decoded UTF-16BE Arabic string on Page 2");

    auto pdfFull = litebrowser::parsePdfFile(pdfPath, 10);
    check(pdfFull.ok && pdfFull.totalPages == 2, "Primary PDF reader parsed 2 pages");
#ifdef __linux__
    check(pdfFull.hasRenderedImages && !pdfFull.pages[0].pngBase64.empty(),
          "Linux Poppler+Cairo rendered pixel-accurate PNG page image");
#endif

    // 5. Unified DocumentViewer HTML generation
    auto viewOut = litebrowser::renderDocumentToHtml(docxPath, 99);
    check(viewOut.ok && litebrowser::fs::fileExists(viewOut.tempHtmlPath),
          "DocumentViewer generated standalone Reader HTML session file");
}

void testStorageAndInternalPages() {
    std::printf("\n--- Testing StorageManager & Internal Pages ---\n");
    std::string tmpCfg = litebrowser::fs::joinPath(litebrowser::fs::getTempSessionDir(), "cfg_test");
    litebrowser::StorageManager st(tmpCfg);

    auto bms = st.getBookmarks();
    check(bms.size() >= 4, "Default Arabic bookmarks initialized");

    st.toggleBookmark("اختبار موقع", "https://example.com");
    check(st.isBookmarked("https://example.com"), "Bookmark added and persisted");
    st.toggleBookmark("اختبار موقع", "https://example.com");
    check(!st.isBookmarked("https://example.com"), "Bookmark toggled off");

    st.addHistory("موسوعة ويكيبيديا", "https://ar.wikipedia.org");
    check(st.getHistory().size() == 1, "History entry recorded");

    int dlId = st.addDownload("setup.exe", "https://example.org/setup.exe", "/tmp/setup.exe");
    st.updateDownloadProgress(dlId, 512000, 1024000);
    st.finishDownload(dlId, litebrowser::DownloadStatus::Completed);
    auto dl = st.getDownloadById(dlId);
    check(dl.status == litebrowser::DownloadStatus::Completed && dl.receivedBytes == 512000,
          "Download lifecycle tracked accurately");

    auto home = litebrowser::renderInternalPage("lite://home", st, 1);
    auto games = litebrowser::renderInternalPage("lite://games", st, 2);
    auto dlPage = litebrowser::renderInternalPage("lite://downloads", st, 3);
    check(home.html.find("متصفح النور الخفيف") != std::string::npos, "lite://home page rendered");
    check(games.html.find("startGame('snake')") != std::string::npos, "lite://games built-in HTML5 games rendered");
    check(dlPage.html.find("setup.exe") != std::string::npos, "lite://downloads lists downloaded program");
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string samplesDir = (argc >= 2) ? argv[1] : "samples";
    testOmniboxAndAdblock();
    testDocuments(samplesDir);
    testStorageAndInternalPages();

    litebrowser::fs::cleanupTempSessionDir();
    std::printf("\n========================================\n");
    std::printf("Passed: %d | Failed: %d\n", g_passed, g_failed);
    std::printf("========================================\n");
    return (g_failed == 0) ? 0 : 1;
}

