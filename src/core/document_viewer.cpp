#include "document_viewer.hpp"

#include "doc_reader.hpp"
#include "docx_reader.hpp"
#include "fs_utils.hpp"
#include "pdf_reader.hpp"

#include <cstdio>
#include <sstream>

namespace litebrowser {

bool isSupportedDocumentFile(const std::string& pathOrUrl) {
    std::string ext = fs::getExtensionLower(pathOrUrl);
    return (ext == ".pdf" || ext == ".docx" || ext == ".doc" ||
            ext == ".rtf" || ext == ".txt" || ext == ".md");
}

namespace {

std::string formatPlainTextBody(const std::string& text) {
    std::string html;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = fs::trim(line);
        if (t.empty()) {
            html += "<div style=\"height:10px;\"></div>\n";
            continue;
        }
        bool rtl = fs::containsArabicText(t);
        if (t.compare(0, 2, "# ") == 0) {
            html += std::string("<h1 dir=\"") + (rtl ? "rtl" : "ltr") + "\">" + fs::htmlEscape(t.substr(2)) + "</h1>\n";
        } else if (t.compare(0, 3, "## ") == 0) {
            html += std::string("<h2 dir=\"") + (rtl ? "rtl" : "ltr") + "\">" + fs::htmlEscape(t.substr(3)) + "</h2>\n";
        } else if (t.compare(0, 4, "### ") == 0) {
            html += std::string("<h3 dir=\"") + (rtl ? "rtl" : "ltr") + "\">" + fs::htmlEscape(t.substr(4)) + "</h3>\n";
        } else {
            html += std::string("<p dir=\"") + (rtl ? "rtl" : "ltr") + "\"" +
                    (rtl ? " style=\"text-align:right;\"" : "") + ">" + fs::htmlEscape(line) + "</p>\n";
        }
    }
    return html;
}

std::string buildReaderShellHtml(const std::string& badgeText,
                                 const std::string& badgeColor,
                                 const std::string& docTitle,
                                 const std::string& metaSubtitle,
                                 const std::string& innerContentHtml) {
    std::string out;
    out.reserve(innerContentHtml.size() + 8192);
    out += "<!DOCTYPE html>\n<html lang=\"ar\" dir=\"rtl\">\n<head>\n"
           "<meta charset=\"UTF-8\"/>\n"
           "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"/>\n"
           "<title>" + fs::htmlEscape(docTitle) + " - القارئ المدمج</title>\n"
           "<style>\n"
           "* { box-sizing: border-box; }\n"
           "body { margin: 0; font-family: Tahoma, 'Segoe UI', 'Noto Sans Arabic', Arial, sans-serif; "
           "background: #eef1f5; color: #1c2430; transition: background 0.15s, color 0.15s; }\n"
           "body.theme-sepia { background: #f4ecd8; color: #3d3122; }\n"
           "body.theme-dark { background: #141820; color: #e2e8f0; }\n"
           ".reader-bar { position: fixed; top: 0; left: 0; right: 0; z-index: 100; "
           "background: #1e293b; color: #f8fafc; padding: 8px 16px; display: flex; flex-wrap: wrap; "
           "align-items: center; justify-content: space-between; gap: 8px; box-shadow: 0 2px 8px rgba(0,0,0,0.22); }\n"
           ".reader-info { display: flex; align-items: center; gap: 10px; min-width: 180px; }\n"
           ".badge { padding: 3px 9px; border-radius: 5px; font-weight: bold; font-size: 12px; color: #fff; background: " + badgeColor + "; }\n"
           ".doc-title { font-weight: bold; font-size: 14px; max-width: 340px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }\n"
           ".doc-meta { font-size: 12px; color: #94a3b8; }\n"
           ".reader-controls { display: flex; align-items: center; gap: 6px; flex-wrap: wrap; }\n"
           ".rbtn { background: #334155; color: #f8fafc; border: 1px solid #475569; border-radius: 5px; "
           "padding: 5px 10px; font-size: 12px; cursor: pointer; font-family: inherit; }\n"
           ".rbtn:hover { background: #475569; }\n"
           ".search-inp { padding: 5px 9px; border-radius: 5px; border: 1px solid #475569; background: #0f172a; "
           "color: #fff; font-size: 12px; width: 170px; font-family: inherit; }\n"
           ".reader-stage { max-width: 900px; margin: 68px auto 40px auto; padding: 0 14px; }\n"
           ".paper-sheet { background: #ffffff; border-radius: 8px; padding: 36px 44px; margin-bottom: 22px; "
           "box-shadow: 0 2px 12px rgba(15,23,42,0.09); line-height: 1.85; font-size: 16px; overflow-wrap: break-word; }\n"
           "body.theme-sepia .paper-sheet { background: #fbf5e6; color: #3b2f1e; }\n"
           "body.theme-dark .paper-sheet { background: #1e2532; color: #e2e8f0; box-shadow: 0 2px 12px rgba(0,0,0,0.45); }\n"
           ".paper-sheet h1, .paper-sheet h2, .paper-sheet h3, .paper-sheet h4 { margin-top: 1.1em; margin-bottom: 0.45em; line-height: 1.4; }\n"
           ".paper-sheet p { margin: 0.55em 0; }\n"
           ".doc-list-item { margin: 0.35em 0; padding: 0 18px; }\n"
           ".doc-table-wrap { overflow-x: auto; margin: 14px 0; }\n"
           ".doc-table { width: 100%; border-collapse: collapse; }\n"
           ".doc-table td, .doc-table th { border: 1px solid #cbd5e1; padding: 8px 10px; vertical-align: top; }\n"
           "body.theme-dark .doc-table td { border-color: #334155; }\n"
           ".doc-img-box { text-align: center; margin: 14px 0; }\n"
           ".doc-img-box img, .pdf-page-img { max-width: 100%; height: auto; border-radius: 4px; }\n"
           ".pdf-page-header { display: flex; justify-content: space-between; font-size: 12px; color: #64748b; "
           "border-bottom: 1px solid #e2e8f0; padding-bottom: 6px; margin-bottom: 14px; }\n"
           "body.theme-dark .pdf-page-header { border-color: #334155; color: #94a3b8; }\n"
           ".search-hit { background: #fde047; color: #0f172a; padding: 0 2px; border-radius: 2px; }\n"
           "@media print { .reader-bar { display: none !important; } .reader-stage { margin: 0; max-width: 100%; } "
           ".paper-sheet { box-shadow: none; padding: 12px; page-break-after: always; } }\n"
           "</style>\n</head>\n<body>\n"
           "<div class=\"reader-bar\">\n"
           "  <div class=\"reader-info\">\n"
           "    <span class=\"badge\">" + fs::htmlEscape(badgeText) + "</span>\n"
           "    <div>\n"
           "      <div class=\"doc-title\" title=\"" + fs::htmlEscape(docTitle) + "\">" + fs::htmlEscape(docTitle) + "</div>\n"
           "      <div class=\"doc-meta\">" + fs::htmlEscape(metaSubtitle) + "</div>\n"
           "    </div>\n"
           "  </div>\n"
           "  <div class=\"reader-controls\">\n"
           "    <input id=\"qBox\" class=\"search-inp\" type=\"text\" placeholder=\"بحث داخل المستند...\" onkeyup=\"onSearchKey(event)\"/>\n"
           "    <button class=\"rbtn\" onclick=\"findInDoc()\">بحث</button>\n"
           "    <span id=\"qStat\" class=\"doc-meta\"></span>\n"
           "    <button class=\"rbtn\" onclick=\"zoomStep(10)\" title=\"تكبير\">تكبير +</button>\n"
           "    <button class=\"rbtn\" onclick=\"zoomStep(-10)\" title=\"تصغير\">تصغير -</button>\n"
           "    <span id=\"zLabel\" class=\"doc-meta\">100%</span>\n"
           "    <button class=\"rbtn\" onclick=\"setTheme('')\">نهاري</button>\n"
           "    <button class=\"rbtn\" onclick=\"setTheme('theme-sepia')\">ورقي</button>\n"
           "    <button class=\"rbtn\" onclick=\"setTheme('theme-dark')\">ليلي</button>\n"
           "    <button class=\"rbtn\" onclick=\"window.print()\">طباعة</button>\n"
           "  </div>\n"
           "</div>\n"
           "<div id=\"docStage\" class=\"reader-stage\">\n" +
           innerContentHtml +
           "</div>\n"
           "<script>\n"
           "var curZoom = 100;\n"
           "function zoomStep(delta) {\n"
           "  curZoom = Math.max(60, Math.min(220, curZoom + delta));\n"
           "  var st = document.getElementById('docStage');\n"
           "  st.style.maxWidth = Math.round(900 * curZoom / 100) + 'px';\n"
           "  var sheets = document.getElementsByClassName('paper-sheet');\n"
           "  for (var i = 0; i < sheets.length; i++) {\n"
           "    sheets[i].style.fontSize = Math.round(16 * curZoom / 100) + 'px';\n"
           "  }\n"
           "  document.getElementById('zLabel').innerHTML = curZoom + '%';\n"
           "}\n"
           "function setTheme(cls) {\n"
           "  document.body.className = cls;\n"
           "}\n"
           "function onSearchKey(e) {\n"
           "  var k = e ? (e.keyCode || e.which) : 0;\n"
           "  if (k === 13) findInDoc();\n"
           "}\n"
           "function findInDoc() {\n"
           "  var q = document.getElementById('qBox').value;\n"
           "  if (!q) { document.getElementById('qStat').innerHTML = ''; return; }\n"
           "  if (window.find) {\n"
           "    var found = window.find(q, false, false, true, false, false, false);\n"
           "    document.getElementById('qStat').innerHTML = found ? 'تم العثور' : 'لا نتائج';\n"
           "    return;\n"
           "  }\n"
           "  var sheets = document.getElementsByClassName('paper-sheet');\n"
           "  var ql = q.toLowerCase();\n"
           "  for (var i = 0; i < sheets.length; i++) {\n"
           "    var txt = (sheets[i].innerText || sheets[i].textContent || '').toLowerCase();\n"
           "    if (txt.indexOf(ql) !== -1) {\n"
           "      sheets[i].scrollIntoView(true);\n"
           "      document.getElementById('qStat').innerHTML = 'موجود في القسم ' + (i + 1);\n"
           "      return;\n"
           "    }\n"
           "  }\n"
           "  document.getElementById('qStat').innerHTML = 'لا نتائج';\n"
           "}\n"
           "</script>\n</body>\n</html>\n";
    return out;
}

}  // namespace

DocumentViewOutput renderDocumentToHtml(const std::string& filePathUtf8, int slotId) {
    DocumentViewOutput out;
    std::string ext = fs::getExtensionLower(filePathUtf8);
    std::string fileName = fs::getFileName(filePathUtf8);

    if (!fs::fileExists(filePathUtf8)) {
        out.tabTitle = "ملف غير موجود";
        out.fullHtml = buildReaderShellHtml(
            "خطأ", "#dc2626", fileName, "تعذّر العثور على الملف",
            "<div class=\"paper-sheet\"><h2 dir=\"rtl\">الملف غير موجود</h2><p dir=\"rtl\">المسار: " +
                fs::htmlEscape(filePathUtf8) + "</p></div>");
    } else if (ext == ".pdf") {
        PdfParseResult pdf = parsePdfFile(filePathUtf8, 60);
        if (!pdf.ok) {
            out.tabTitle = fileName;
            out.fullHtml = buildReaderShellHtml(
                "PDF", "#dc2626", fileName, "تعذّر قراءة ملف PDF",
                "<div class=\"paper-sheet\"><p dir=\"rtl\">" + fs::htmlEscape(pdf.errorMessage) + "</p></div>");
        } else {
            out.ok = true;
            out.tabTitle = pdf.title + " (PDF)";
            std::string subtitle = "عدد الصفحات: " + std::to_string(pdf.totalPages);
            std::string pagesHtml;

            for (const auto& pg : pdf.pages) {
                pagesHtml += "<div class=\"paper-sheet\" id=\"page-" + std::to_string(pg.pageNumber) + "\">\n";
                pagesHtml += "  <div class=\"pdf-page-header\"><span>صفحة " + std::to_string(pg.pageNumber) +
                             " من " + std::to_string(pdf.totalPages) + "</span><span>" +
                             fs::htmlEscape(fileName) + "</span></div>\n";
                if (!pg.pngBase64.empty()) {
                    pagesHtml += "  <div style=\"text-align:center;\"><img class=\"pdf-page-img\" src=\"data:image/png;base64," +
                                 pg.pngBase64 + "\" alt=\"صفحة " + std::to_string(pg.pageNumber) + "\"/></div>\n";
                    if (!fs::trim(pg.textUtf8).empty()) {
                        pagesHtml += "  <details style=\"margin-top:10px;font-size:13px;\"><summary style=\"cursor:pointer;color:#64748b;\">"
                                     "عرض النص المستخرج للنسخ والبحث</summary><div style=\"margin-top:8px;\">" +
                                     formatPlainTextBody(pg.textUtf8) + "</div></details>\n";
                    }
                } else {
                    for (const auto& jb : pg.jpegImagesBase64) {
                        pagesHtml += "  <div class=\"doc-img-box\"><img src=\"data:image/jpeg;base64," + jb + "\"/></div>\n";
                    }
                    if (!fs::trim(pg.textUtf8).empty()) {
                        pagesHtml += formatPlainTextBody(pg.textUtf8);
                    } else if (pg.jpegImagesBase64.empty()) {
                        std::string rawFileUrl = fs::pathToFileUrl(filePathUtf8);
                        pagesHtml += "<p dir=\"rtl\" style=\"color:#64748b;\">هذه الصفحة تحتوي على رسوميات متجهة أو صور مضغوطة. "
                                     "<a href=\"" + fs::htmlEscape(rawFileUrl) + "\">افتح عبر عارض النظام المباشر</a></p>\n";
                    }
                }
                pagesHtml += "</div>\n";
            }
            out.fullHtml = buildReaderShellHtml("PDF", "#dc2626", pdf.title, subtitle, pagesHtml);
        }
    } else if (ext == ".docx" || ext == ".doc" || ext == ".rtf") {
        DocxParseResult doc = (ext == ".docx") ? parseDocxFile(filePathUtf8)
                                               : parseLegacyDocOrRtfFile(filePathUtf8);
        if (!doc.ok) {
            // Try fallback in case a legacy .doc was renamed to .docx or vice versa
            doc = parseLegacyDocOrRtfFile(filePathUtf8);
        }
        if (!doc.ok) {
            out.tabTitle = fileName;
            out.fullHtml = buildReaderShellHtml(
                "WORD", "#2563eb", fileName, "تعذّر قراءة المستند",
                "<div class=\"paper-sheet\"><p dir=\"rtl\">" + fs::htmlEscape(doc.errorMessage) + "</p></div>");
        } else {
            out.ok = true;
            out.tabTitle = doc.title + " (Word)";
            std::string subtitle = "الفقرات: " + std::to_string(doc.paragraphCount) +
                                   " | الكلمات: " + std::to_string(doc.wordCount);
            std::string sheet = "<div class=\"paper-sheet\">\n" +
                                (doc.bodyHtml.empty() ? std::string("<p dir=\"rtl\" style=\"color:#64748b;\">(مستند فارغ)</p>") : doc.bodyHtml) +
                                "\n</div>\n";
            out.fullHtml = buildReaderShellHtml("WORD", "#2563eb", doc.title, subtitle, sheet);
        }
    } else {
        std::string txt;
        if (fs::readFileText(filePathUtf8, txt)) {
            out.ok = true;
            out.tabTitle = fileName;
            std::string sheet = "<div class=\"paper-sheet\">\n" + formatPlainTextBody(txt) + "\n</div>\n";
            out.fullHtml = buildReaderShellHtml("TEXT", "#059669", fileName, fs::formatBytes(txt.size()), sheet);
        }
    }

    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "reader_view_%d.html", slotId);
    out.tempHtmlPath = fs::joinPath(fs::getTempSessionDir(), nameBuf);
    fs::writeFileText(out.tempHtmlPath, out.fullHtml);
    out.tempFileUrl = fs::pathToFileUrl(out.tempHtmlPath);
    return out;
}

}  // namespace litebrowser

