#include "document_viewer.hpp"

#include "doc_reader.hpp"
#include "docx_reader.hpp"
#include "fs_utils.hpp"
#include "i18n.hpp"
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
                                 const std::string& innerContentHtml,
                                 const std::string& theme = "dark",
                                 const std::string& lang = "ar") {
    bool en = (lang == "en");
    std::string dir = en ? "ltr" : "rtl";
    std::string initialBodyClass = (theme == "dark") ? "theme-dark" : "";

    std::string out;
    out.reserve(innerContentHtml.size() + 10240);
    out += "<!DOCTYPE html>\n<!-- saved from url=(0014)about:internet -->\n<html lang=\"" + lang + "\" dir=\"" + dir + "\">\n<head>\n"
           "<meta charset=\"UTF-8\"/>\n"
           "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"/>\n"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>\n"
           "<title>" + fs::htmlEscape(docTitle) + (en ? " - Document Reader" : " - القارئ المدمج") + "</title>\n"
           "<style>\n"
           ":root {\n"
           "  --bg-app: #f1f5f9;\n"
           "  --bg-sheet: #ffffff;\n"
           "  --text-main: #0f172a;\n"
           "  --text-muted: #64748b;\n"
           "  --border-sheet: rgba(226, 232, 240, 0.85);\n"
           "  --sheet-shadow: 0 4px 6px -1px rgba(15,23,42,0.05), 0 20px 25px -5px rgba(15,23,42,0.07);\n"
           "}\n"
           "* { box-sizing: border-box; margin: 0; padding: 0; }\n"
           "body {\n"
           "  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Tahoma, 'Noto Sans Arabic', Arial, sans-serif;\n"
           "  background-color: var(--bg-app);\n"
           "  color: var(--text-main);\n"
           "  line-height: 1.85;\n"
           "  min-height: 100vh;\n"
           "  -webkit-font-smoothing: antialiased;\n"
           "  transition: background-color 0.2s ease, color 0.2s ease;\n"
           "}\n"
           "/* Reading Themes */\n"
           "body.theme-sepia {\n"
           "  --bg-app: #ede5d0;\n"
           "  --bg-sheet: #fcf7eb;\n"
           "  --text-main: #3d301f;\n"
           "  --text-muted: #78644e;\n"
           "  --border-sheet: #dfd4be;\n"
           "  --sheet-shadow: 0 4px 6px -1px rgba(61,48,31,0.06), 0 20px 25px -5px rgba(61,48,31,0.08);\n"
           "}\n"
           "body.theme-dark {\n"
           "  --bg-app: #090d16;\n"
           "  --bg-sheet: #131b2a;\n"
           "  --text-main: #f1f5f9;\n"
           "  --text-muted: #94a3b8;\n"
           "  --border-sheet: #1e293b;\n"
           "  --sheet-shadow: 0 4px 6px -1px rgba(0,0,0,0.3), 0 20px 25px -5px rgba(0,0,0,0.5);\n"
           "}\n"
           "/* Reading Progress Bar */\n"
           "#readProgress {\n"
           "  position: fixed; top: 0; left: 0; height: 3px;\n"
           "  background: linear-gradient(90deg, #38bdf8, #818cf8);\n"
           "  width: 0%; z-index: 1001; transition: width 0.1s ease;\n"
           "}\n"
           "/* Frosted Glass Top Toolbar */\n"
           ".reader-bar {\n"
           "  position: fixed; top: 0; left: 0; right: 0; z-index: 1000;\n"
           "  background: #0f172a;\n"
           "  background: rgba(15, 23, 42, 0.92);\n"
           "  backdrop-filter: blur(14px);\n"
           "  -webkit-backdrop-filter: blur(14px);\n"
           "  color: #f8fafc; padding: 9px 18px;\n"
           "  display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 10px;\n"
           "  border-bottom: 1px solid rgba(255, 255, 255, 0.08);\n"
           "  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);\n"
           "  *zoom: 1;\n"
           "}\n"
           ".reader-bar:after { content: ''; display: table; clear: both; }\n"
           ".reader-info { display: flex; align-items: center; gap: 12px; min-width: 180px; }\n"
           ".badge {\n"
           "  padding: 4px 10px; border-radius: 9999px; font-weight: 800; font-size: 11px;\n"
           "  letter-spacing: 0.5px; color: #ffffff;\n"
           "  background-color: " + badgeColor + ";\n"
           "  box-shadow: 0 2px 6px rgba(0,0,0,0.2);\n"
           "}\n"
           ".doc-title {\n"
           "  font-weight: 700; font-size: 14px; max-width: 320px;\n"
           "  white-space: nowrap; overflow: hidden; text-overflow: ellipsis; color: #f8fafc;\n"
           "}\n"
           ".doc-meta { font-size: 11px; color: #94a3b8; }\n"
           ".reader-controls { display: flex; align-items: center; gap: 6px; flex-wrap: wrap; }\n"
           ".btn-group { display: inline-flex; align-items: center; background: rgba(255,255,255,0.06); border: 1px solid rgba(255,255,255,0.1); border-radius: 9999px; padding: 2px; gap: 2px; }\n"
           ".rbtn {\n"
           "  background: rgba(255, 255, 255, 0.08); color: #f8fafc;\n"
           "  border: 1px solid rgba(255, 255, 255, 0.12); border-radius: 6px;\n"
           "  padding: 5px 10px; font-size: 12px; font-weight: 600; cursor: pointer;\n"
           "  font-family: inherit; transition: all 0.15s ease; outline: none;\n"
           "}\n"
           ".rbtn:hover { background: rgba(255, 255, 255, 0.18); border-color: rgba(255, 255, 255, 0.25); transform: translateY(-1px); }\n"
           ".rbtn.active { background: #0284c7; border-color: #38bdf8; color: #ffffff; }\n"
           ".rbtn-pill { border-radius: 9999px; border: none; background: transparent; padding: 4px 10px; }\n"
           ".rbtn-pill:hover { background: rgba(255, 255, 255, 0.15); }\n"
           ".rbtn-pill.active { background: #0284c7; color: #ffffff; }\n"
           ".search-inp {\n"
           "  padding: 5px 12px; border-radius: 9999px;\n"
           "  border: 1px solid rgba(255, 255, 255, 0.15);\n"
           "  background: rgba(15, 23, 42, 0.8);\n"
           "  color: #ffffff; font-size: 12px; width: 160px; font-family: inherit; outline: none;\n"
           "  transition: border-color 0.15s, box-shadow 0.15s;\n"
           "}\n"
           ".search-inp:focus { border-color: #38bdf8; box-shadow: 0 0 0 2px rgba(56, 189, 248, 0.25); }\n"
           ".search-inp::placeholder { color: #64748b; }\n"
           "/* Main Document Stage */\n"
           ".reader-stage {\n"
           "  max-width: 880px; margin: 76px auto 50px auto; padding: 0 16px;\n"
           "  transition: max-width 0.2s ease;\n"
           "}\n"
           "/* Realistic Paper Sheet */\n"
           ".paper-sheet {\n"
           "  background-color: var(--bg-sheet);\n"
           "  border: 1px solid var(--border-sheet);\n"
           "  border-radius: 12px;\n"
           "  padding: 42px 48px;\n"
           "  margin-bottom: 26px;\n"
           "  box-shadow: var(--sheet-shadow);\n"
           "  line-height: 1.85;\n"
           "  font-size: 16px;\n"
           "  overflow-wrap: break-word;\n"
           "  position: relative;\n"
           "  transition: background-color 0.2s ease, border-color 0.2s ease, box-shadow 0.2s ease;\n"
           "}\n"
           ".paper-sheet h1, .paper-sheet h2, .paper-sheet h3, .paper-sheet h4 {\n"
           "  margin-top: 1.2em; margin-bottom: 0.5em; line-height: 1.4; color: inherit;\n"
           "}\n"
           ".paper-sheet h1 { font-size: 26px; border-bottom: 1px solid var(--border-sheet); padding-bottom: 8px; }\n"
           ".paper-sheet h2 { font-size: 21px; }\n"
           ".paper-sheet h3 { font-size: 18px; }\n"
           ".paper-sheet p { margin: 0.65em 0; }\n"
           ".doc-list-item { margin: 0.35em 0; padding: 0 20px; }\n"
           ".doc-table-wrap { overflow-x: auto; margin: 18px 0; border-radius: 8px; border: 1px solid var(--border-sheet); }\n"
           ".doc-table { width: 100%; border-collapse: collapse; }\n"
           ".doc-table td, .doc-table th { border: 1px solid var(--border-sheet); padding: 10px 14px; vertical-align: top; font-size: 14px; }\n"
           ".doc-table th { background: rgba(0,0,0,0.03); font-weight: 700; }\n"
           ".doc-img-box { text-align: center; margin: 20px 0; }\n"
           ".doc-img-box img, .pdf-page-img {\n"
           "  max-width: 100%; height: auto; border-radius: 6px;\n"
           "  box-shadow: 0 2px 8px rgba(0,0,0,0.08); transition: transform 0.2s ease;\n"
           "}\n"
           ".pdf-page-header {\n"
           "  display: flex; justify-content: space-between; font-size: 12px; color: var(--text-muted);\n"
           "  border-bottom: 1px solid var(--border-sheet); padding-bottom: 8px; margin-bottom: 16px;\n"
           "}\n"
           ".search-hit { background: #fde047; color: #0f172a; padding: 1px 4px; border-radius: 3px; font-weight: bold; }\n"
           "/* Print Styles */\n"
           "@media print {\n"
           "  #readProgress, .reader-bar { display: none !important; }\n"
           "  .reader-stage { margin: 0; max-width: 100% !important; padding: 0; }\n"
           "  .paper-sheet { box-shadow: none !important; border: none !important; padding: 16px; page-break-after: always; }\n"
           "}\n"
           "</style>\n</head>\n<body class=\"" + initialBodyClass + "\">\n"
           "<div id=\"readProgress\"></div>\n"
           "<div class=\"reader-bar\">\n"
           "  <div class=\"reader-info\">\n"
           "    <span class=\"badge\">" + fs::htmlEscape(badgeText) + "</span>\n"
           "    <div>\n"
           "      <div class=\"doc-title\" title=\"" + fs::htmlEscape(docTitle) + "\">" + fs::htmlEscape(docTitle) + "</div>\n"
           "      <div class=\"doc-meta\">" + fs::htmlEscape(metaSubtitle) + "</div>\n"
           "    </div>\n"
           "  </div>\n"
           "  <div class=\"reader-controls\">\n"
           "    <input id=\"qBox\" class=\"search-inp\" type=\"text\" placeholder=\"" +
           fs::htmlEscape(i18n::get("doc_search_inp", lang)) + "\" onkeyup=\"onSearchKey(event)\"/>\n"
           "    <button class=\"rbtn\" onclick=\"findInDoc()\">" + fs::htmlEscape(i18n::get("doc_search_btn", lang)) + "</button>\n"
           "    <span id=\"qStat\" class=\"doc-meta\"></span>\n"
           "    <input id=\"jumpInp\" class=\"search-inp\" style=\"width:54px;text-align:center;\" type=\"number\" min=\"1\" placeholder=\"" +
           (en ? "P..." : "ص...") + "\" onchange=\"jumpToPage(this.value)\" title=\"" + (en ? "Jump to page" : "انتقال إلى صفحة") + "\"/>\n"
           "    <button class=\"rbtn\" onclick=\"rotatePages()\" title=\"" + (en ? "Rotate pages 90 deg" : "تدوير الصفحات 90 درجة") + "\">" +
           fs::htmlEscape(i18n::get("doc_rotate_btn", lang)) + "</button>\n"
           "    <div class=\"btn-group\">\n"
           "      <button class=\"rbtn rbtn-pill\" onclick=\"zoomStep(-10)\" title=\"" + (en ? "Zoom Out" : "تصغير الخط") + "\">A-</button>\n"
           "      <span id=\"zLabel\" class=\"doc-meta\" style=\"padding:0 6px;\">100%</span>\n"
           "      <button class=\"rbtn rbtn-pill\" onclick=\"zoomStep(10)\" title=\"" + (en ? "Zoom In" : "تكبير الخط") + "\">A+</button>\n"
           "    </div>\n"
           "    <div class=\"btn-group\">\n"
           "      <button id=\"btnDay\" class=\"rbtn rbtn-pill " + (theme != "dark" ? "active" : "") + "\" onclick=\"setTheme('', this)\" title=\"" + (en ? "Day Mode" : "الوضع النهاري") + "\">" +
           fs::htmlEscape(i18n::get("doc_day", lang)) + "</button>\n"
           "      <button id=\"btnSepia\" class=\"rbtn rbtn-pill\" onclick=\"setTheme('theme-sepia', this)\" title=\"" + (en ? "Sepia / Book Mode" : "الوضع الورقي / سيبيا") + "\">" +
           fs::htmlEscape(i18n::get("doc_sepia", lang)) + "</button>\n"
           "      <button id=\"btnDark\" class=\"rbtn rbtn-pill " + (theme == "dark" ? "active" : "") + "\" onclick=\"setTheme('theme-dark', this)\" title=\"" + (en ? "Dark Mode" : "الوضع الليلي") + "\">" +
           fs::htmlEscape(i18n::get("doc_dark", lang)) + "</button>\n"
           "    </div>\n"
           "    <button class=\"rbtn\" onclick=\"window.print()\" title=\"" + (en ? "Print document" : "طباعة المستند") + "\">" +
           fs::htmlEscape(i18n::get("doc_print", lang)) + "</button>\n"
           "  </div>\n"
           "</div>\n"
           "<div id=\"docStage\" class=\"reader-stage\">\n" +
           innerContentHtml +
           "</div>\n"
           "<script>\n"
           "var curZoom = 100;\n"
           "var curRot = 0;\n"
           "window.onscroll = function() {\n"
           "  var s = document.documentElement.scrollTop || document.body.scrollTop;\n"
           "  var h = (document.documentElement.scrollHeight || document.body.scrollHeight) - (document.documentElement.clientHeight || window.innerHeight);\n"
           "  var pct = h > 0 ? Math.min(100, Math.round((s / h) * 100)) : 0;\n"
           "  var bar = document.getElementById('readProgress');\n"
           "  if (bar) bar.style.width = pct + '%';\n"
           "};\n"
           "function rotatePages() {\n"
           "  curRot = (curRot + 90) % 360;\n"
           "  var imgs = document.getElementsByClassName('pdf-page-img');\n"
           "  for (var i = 0; i < imgs.length; i++) {\n"
           "    imgs[i].style.transform = 'rotate(' + curRot + 'deg)';\n"
           "  }\n"
           "}\n"
           "function jumpToPage(num) {\n"
           "  var el = document.getElementById('page-' + num);\n"
           "  if (el) el.scrollIntoView(true);\n"
           "}\n"
           "function zoomStep(delta) {\n"
           "  curZoom = Math.max(60, Math.min(220, curZoom + delta));\n"
           "  var st = document.getElementById('docStage');\n"
           "  if (st) st.style.maxWidth = Math.round(880 * curZoom / 100) + 'px';\n"
           "  var sheets = document.getElementsByClassName('paper-sheet');\n"
           "  for (var i = 0; i < sheets.length; i++) {\n"
           "    sheets[i].style.fontSize = Math.round(16 * curZoom / 100) + 'px';\n"
           "  }\n"
           "  var lbl = document.getElementById('zLabel');\n"
           "  if (lbl) lbl.innerHTML = curZoom + '%';\n"
           "}\n"
           "function setTheme(cls, btn) {\n"
           "  document.body.className = cls;\n"
           "  var btns = ['btnDay', 'btnSepia', 'btnDark'];\n"
           "  for (var i = 0; i < btns.length; i++) {\n"
           "    var b = document.getElementById(btns[i]);\n"
           "    if (b) b.className = 'rbtn rbtn-pill';\n"
           "  }\n"
           "  if (btn) btn.className = 'rbtn rbtn-pill active';\n"
           "}\n"
           "function onSearchKey(e) {\n"
           "  var k = e ? (e.keyCode || e.which) : 0;\n"
           "  if (k === 13) findInDoc();\n"
           "}\n"
           "function findInDoc() {\n"
           "  var q = document.getElementById('qBox').value;\n"
           "  var stat = document.getElementById('qStat');\n"
           "  if (!q) { if (stat) stat.innerHTML = ''; return; }\n"
           "  if (window.find) {\n"
           "    var found = window.find(q, false, false, true, false, false, false);\n"
           "    if (stat) stat.innerHTML = found ? '" + fs::htmlEscape(i18n::get("doc_found", lang)) + "' : '" +
           fs::htmlEscape(i18n::get("doc_not_found", lang)) + "';\n"
           "    return;\n"
           "  }\n"
           "  var sheets = document.getElementsByClassName('paper-sheet');\n"
           "  var ql = q.toLowerCase();\n"
           "  for (var i = 0; i < sheets.length; i++) {\n"
           "    var txt = (sheets[i].innerText || sheets[i].textContent || '').toLowerCase();\n"
           "    if (txt.indexOf(ql) !== -1) {\n"
           "      sheets[i].scrollIntoView(true);\n"
           "      if (stat) stat.innerHTML = '" + fs::htmlEscape(i18n::get("doc_in_section", lang)) + "' + (i + 1);\n"
           "      return;\n"
           "    }\n"
           "  }\n"
           "  if (stat) stat.innerHTML = '" + fs::htmlEscape(i18n::get("doc_not_found", lang)) + "';\n"
           "}\n"
           "</script>\n</body>\n</html>\n";
    return out;
}

}  // namespace

DocumentViewOutput renderDocumentToHtml(const std::string& filePathUtf8, int slotId,
                                        const std::string& theme,
                                        const std::string& lang) {
    DocumentViewOutput out;
    std::string ext = fs::getExtensionLower(filePathUtf8);
    std::string fileName = fs::getFileName(filePathUtf8);

    if (!fs::fileExists(filePathUtf8)) {
        out.tabTitle = (lang == "en") ? "File Not Found" : "ملف غير موجود";
        out.fullHtml = buildReaderShellHtml(
            (lang == "en") ? "Error" : "خطأ", "#dc2626", fileName,
            (lang == "en") ? "File could not be found" : "تعذّر العثور على الملف",
            "<div class=\"paper-sheet\"><h2 dir=\"" + std::string(lang == "en" ? "ltr" : "rtl") + "\">" +
                out.tabTitle + "</h2><p dir=\"" + (lang == "en" ? "ltr" : "rtl") + "\">Path: " +
                fs::htmlEscape(filePathUtf8) + "</p></div>", theme, lang);
    } else if (ext == ".pdf") {
        PdfParseResult pdf = parsePdfFile(filePathUtf8, 60);
        if (!pdf.ok) {
            out.tabTitle = fileName;
            std::string sub = pdf.isEncrypted ? ((lang == "en") ? "Password Protected File" : "ملف محمي بكلمة مرور")
                                              : ((lang == "en") ? "Failed to parse PDF" : "تعذّر قراءة ملف PDF");
            out.fullHtml = buildReaderShellHtml(
                "PDF", "#dc2626", fileName, sub,
                "<div class=\"paper-sheet\"><h3 dir=\"" + std::string(lang == "en" ? "ltr" : "rtl") +
                "\" style=\"color:#dc2626;\">" + ((lang == "en") ? "Notice" : "تنبيه") + "</h3><p dir=\"" +
                (lang == "en" ? "ltr" : "rtl") + "\">" + fs::htmlEscape(pdf.errorMessage) + "</p></div>", theme, lang);
        } else {
            out.ok = true;
            out.tabTitle = pdf.title + " (PDF)";
            std::string subtitle = (lang == "en") ? ("Pages: " + std::to_string(pdf.totalPages))
                                                  : ("عدد الصفحات: " + std::to_string(pdf.totalPages));
            std::string pagesHtml;

            for (const auto& pg : pdf.pages) {
                pagesHtml += "<div class=\"paper-sheet\" id=\"page-" + std::to_string(pg.pageNumber) + "\">\n";
                pagesHtml += "  <div class=\"pdf-page-header\"><span>" +
                             ((lang == "en") ? ("Page " + std::to_string(pg.pageNumber) + " of " + std::to_string(pdf.totalPages))
                                             : ("صفحة " + std::to_string(pg.pageNumber) + " من " + std::to_string(pdf.totalPages))) +
                             "</span><span>" + fs::htmlEscape(fileName) + "</span></div>\n";
                if (!pg.imageFileUrl.empty() || !pg.pngBase64.empty()) {
                    std::string imgSrc = pg.imageFileUrl;
                    if (imgSrc.empty() && !pg.pngBase64.empty()) {
                        std::vector<uint8_t> dec = fs::base64Decode(pg.pngBase64);
                        if (!dec.empty()) {
                            std::string pPath = fs::joinPath(fs::getTempSessionDir(), "pdf_p" + std::to_string(pg.pageNumber) + ".png");
                            fs::writeFileBytes(pPath, dec.data(), dec.size());
                            imgSrc = fs::pathToFileUrl(pPath);
                        }
                    }
                    if (imgSrc.empty()) {
                        imgSrc = "data:image/png;base64," + pg.pngBase64;
                    }
                    pagesHtml += "  <div style=\"text-align:center;\"><img class=\"pdf-page-img\" loading=\"lazy\" src=\"" + imgSrc + "\" alt=\"Page " + std::to_string(pg.pageNumber) + "\"/></div>\n";
                    if (!fs::trim(pg.textUtf8).empty()) {
                        pagesHtml += "  <details style=\"margin-top:10px;font-size:13px;\"><summary style=\"cursor:pointer;color:#64748b;\">" +
                                     std::string(i18n::get("doc_show_extracted", lang)) + "</summary><div style=\"margin-top:8px;\">" +
                                     formatPlainTextBody(pg.textUtf8) + "</div></details>\n";
                    }
                } else {
                    for (size_t ji = 0; ji < pg.jpegImagesBase64.size(); ++ji) {
                        const auto& jb = pg.jpegImagesBase64[ji];
                        std::vector<uint8_t> dec = fs::base64Decode(jb);
                        std::string jSrc;
                        if (!dec.empty()) {
                            std::string jPath = fs::joinPath(fs::getTempSessionDir(), "pdf_p" + std::to_string(pg.pageNumber) + "_j" + std::to_string(ji) + ".jpg");
                            fs::writeFileBytes(jPath, dec.data(), dec.size());
                            jSrc = fs::pathToFileUrl(jPath);
                        } else {
                            jSrc = "data:image/jpeg;base64," + jb;
                        }
                        pagesHtml += "  <div class=\"doc-img-box\"><img src=\"" + jSrc + "\"/></div>\n";
                    }
                    if (!fs::trim(pg.textUtf8).empty()) {
                        pagesHtml += formatPlainTextBody(pg.textUtf8);
                    } else if (pg.jpegImagesBase64.empty()) {
                        std::string rawFileUrl = fs::pathToFileUrl(filePathUtf8);
                        pagesHtml += "<p dir=\"" + std::string(lang == "en" ? "ltr" : "rtl") + "\" style=\"color:#64748b;\">" +
                                     ((lang == "en") ? "This page contains compressed vector graphics. "
                                                     : "هذه الصفحة تحتوي على رسوميات متجهة أو صور مضغوطة. ") +
                                     "<a href=\"" + fs::htmlEscape(rawFileUrl) + "\">" +
                                     ((lang == "en") ? "Open with system viewer" : "افتح عبر عارض النظام المباشر") + "</a></p>\n";
                    }
                }
                pagesHtml += "</div>\n";
            }
            out.fullHtml = buildReaderShellHtml("PDF", "#dc2626", pdf.title, subtitle, pagesHtml, theme, lang);
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
                "WORD", "#2563eb", fileName, (lang == "en") ? "Failed to read document" : "تعذّر قراءة المستند",
                "<div class=\"paper-sheet\"><p dir=\"" + std::string(lang == "en" ? "ltr" : "rtl") + "\">" +
                fs::htmlEscape(doc.errorMessage) + "</p></div>", theme, lang);
        } else {
            out.ok = true;
            out.tabTitle = doc.title + " (Word)";
            std::string subtitle = (lang == "en") ? ("Paragraphs: " + std::to_string(doc.paragraphCount) + " | Words: " + std::to_string(doc.wordCount))
                                                  : ("الفقرات: " + std::to_string(doc.paragraphCount) + " | الكلمات: " + std::to_string(doc.wordCount));
            std::string sheet = "<div class=\"paper-sheet\">\n" +
                                (doc.bodyHtml.empty() ? std::string("<p dir=\"" + std::string(lang == "en" ? "ltr" : "rtl") + "\" style=\"color:#64748b;\">(" + ((lang == "en") ? "Empty document" : "مستند فارغ") + ")</p>") : doc.bodyHtml) +
                                "\n</div>\n";
            out.fullHtml = buildReaderShellHtml("WORD", "#2563eb", doc.title, subtitle, sheet, theme, lang);
        }
    } else {
        std::string txt;
        if (fs::readFileText(filePathUtf8, txt)) {
            out.ok = true;
            out.tabTitle = fileName;
            std::string sheet = "<div class=\"paper-sheet\">\n" + formatPlainTextBody(txt) + "\n</div>\n";
            out.fullHtml = buildReaderShellHtml("TEXT", "#059669", fileName, fs::formatBytes(txt.size()), sheet, theme, lang);
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

