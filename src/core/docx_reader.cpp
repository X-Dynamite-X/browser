#include "docx_reader.hpp"

#include "fs_utils.hpp"
#include "zip_reader.hpp"

#include <cctype>
#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace litebrowser {

namespace {

void appendUtf8Codepoint(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string xmlUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            size_t semi = s.find(';', i + 1);
            if (semi != std::string::npos && semi - i <= 12) {
                std::string ent = s.substr(i + 1, semi - i - 1);
                if (ent == "amp") { out.push_back('&'); i = semi; continue; }
                if (ent == "lt") { out.push_back('<'); i = semi; continue; }
                if (ent == "gt") { out.push_back('>'); i = semi; continue; }
                if (ent == "quot") { out.push_back('"'); i = semi; continue; }
                if (ent == "apos") { out.push_back('\''); i = semi; continue; }
                if (!ent.empty() && ent[0] == '#') {
                    uint32_t cp = 0;
                    if (ent.size() > 2 && (ent[1] == 'x' || ent[1] == 'X')) {
                        cp = static_cast<uint32_t>(std::strtoul(ent.c_str() + 2, nullptr, 16));
                    } else {
                        cp = static_cast<uint32_t>(std::strtoul(ent.c_str() + 1, nullptr, 10));
                    }
                    if (cp > 0) {
                        appendUtf8Codepoint(out, cp);
                        i = semi;
                        continue;
                    }
                }
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

struct XmlToken {
    enum Type { OpenTag, CloseTag, SelfCloseTag, Text };
    Type type;
    std::string name;
    std::unordered_map<std::string, std::string> attrs;
    std::string text;
};

void parseTagAttributes(const std::string& raw, std::string& tagName,
                        std::unordered_map<std::string, std::string>& attrs) {
    attrs.clear();
    size_t n = raw.size();
    size_t i = 0;
    while (i < n && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
    size_t nameStart = i;
    while (i < n && !std::isspace(static_cast<unsigned char>(raw[i])) && raw[i] != '/' && raw[i] != '>') ++i;
    tagName = raw.substr(nameStart, i - nameStart);

    while (i < n) {
        while (i < n && (std::isspace(static_cast<unsigned char>(raw[i])) || raw[i] == '/')) ++i;
        if (i >= n) break;
        size_t kStart = i;
        while (i < n && raw[i] != '=' && !std::isspace(static_cast<unsigned char>(raw[i])) && raw[i] != '/') ++i;
        std::string key = raw.substr(kStart, i - kStart);
        while (i < n && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
        std::string val;
        if (i < n && raw[i] == '=') {
            ++i;
            while (i < n && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
            if (i < n && (raw[i] == '"' || raw[i] == '\'')) {
                char q = raw[i++];
                size_t vStart = i;
                while (i < n && raw[i] != q) ++i;
                val = xmlUnescape(raw.substr(vStart, i - vStart));
                if (i < n && raw[i] == q) ++i;
            }
        }
        if (!key.empty()) attrs[key] = val;
    }
}

std::vector<XmlToken> tokenizeXml(const std::string& xml) {
    std::vector<XmlToken> tokens;
    size_t i = 0;
    size_t n = xml.size();
    while (i < n) {
        if (xml[i] == '<') {
            if (xml.compare(i, 4, "<!--") == 0) {
                size_t end = xml.find("-->", i + 4);
                i = (end == std::string::npos) ? n : end + 3;
                continue;
            }
            if (xml.compare(i, 2, "<?") == 0) {
                size_t end = xml.find("?>", i + 2);
                i = (end == std::string::npos) ? n : end + 2;
                continue;
            }
            size_t gt = xml.find('>', i + 1);
            if (gt == std::string::npos) break;
            std::string inside = xml.substr(i + 1, gt - i - 1);
            i = gt + 1;
            if (inside.empty()) continue;

            if (inside[0] == '/') {
                XmlToken tok;
                tok.type = XmlToken::CloseTag;
                tok.name = fs::trim(inside.substr(1));
                tokens.push_back(std::move(tok));
            } else {
                bool selfClose = (!inside.empty() && inside.back() == '/');
                if (selfClose) inside.pop_back();
                XmlToken tok;
                tok.type = selfClose ? XmlToken::SelfCloseTag : XmlToken::OpenTag;
                parseTagAttributes(inside, tok.name, tok.attrs);
                tokens.push_back(std::move(tok));
            }
        } else {
            size_t lt = xml.find('<', i);
            if (lt == std::string::npos) lt = n;
            XmlToken tok;
            tok.type = XmlToken::Text;
            tok.text = xmlUnescape(xml.substr(i, lt - i));
            tokens.push_back(std::move(tok));
            i = lt;
        }
    }
    return tokens;
}

std::string getAttrAnyNs(const std::unordered_map<std::string, std::string>& attrs, const std::string& localKey) {
    auto it = attrs.find(localKey);
    if (it != attrs.end()) return it->second;
    for (const auto& kv : attrs) {
        size_t colon = kv.first.find(':');
        if (colon != std::string::npos && kv.first.substr(colon + 1) == localKey) {
            return kv.second;
        }
    }
    return "";
}

std::string localTagName(const std::string& fullName) {
    size_t colon = fullName.find(':');
    return (colon == std::string::npos) ? fullName : fullName.substr(colon + 1);
}

std::string guessImageMime(const std::string& path) {
    std::string ext = fs::getExtensionLower(path);
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".webp") return "image/webp";
    if (ext == ".bmp") return "image/bmp";
    return "image/png";
}

size_t countWordsUtf8(const std::string& text) {
    size_t count = 0;
    bool inWord = false;
    for (unsigned char c : text) {
        if (c <= 32) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            ++count;
        }
    }
    return count;
}

struct RelTarget {
    std::string target;
    bool isExternal = false;
};

}  // namespace

DocxParseResult parseDocxFile(const std::string& filePathUtf8) {
    DocxParseResult res;
    res.title = fs::getFileName(filePathUtf8);

    ZipArchive zip;
    if (!zip.openFromFile(filePathUtf8)) {
        res.errorMessage = "تعذّر فتح أرشيف ملف Word (.docx). قد يكون الملف تالفاً أو بصيغة مختلفة.";
        return res;
    }

    std::string docXml;
    if (!zip.extractEntryText("word/document.xml", docXml)) {
        res.errorMessage = "لا يحتوي الملف على مستند Word صالح (word/document.xml مفقود).";
        return res;
    }

    // Parse relationships (images & external hyperlinks)
    std::unordered_map<std::string, RelTarget> rels;
    std::string relsXml;
    if (zip.extractEntryText("word/_rels/document.xml.rels", relsXml)) {
        auto relTokens = tokenizeXml(relsXml);
        for (const auto& t : relTokens) {
            if ((t.type == XmlToken::OpenTag || t.type == XmlToken::SelfCloseTag) &&
                localTagName(t.name) == "Relationship") {
                std::string id = getAttrAnyNs(t.attrs, "Id");
                std::string target = getAttrAnyNs(t.attrs, "Target");
                std::string mode = getAttrAnyNs(t.attrs, "TargetMode");
                if (!id.empty() && !target.empty()) {
                    rels[id] = RelTarget{target, fs::toLowerAscii(mode) == "external"};
                }
            }
        }
    }

    // Optional core properties for document title
    std::string coreXml;
    if (zip.extractEntryText("docProps/core.xml", coreXml)) {
        auto coreTokens = tokenizeXml(coreXml);
        bool inTitle = false;
        for (const auto& t : coreTokens) {
            if (t.type == XmlToken::OpenTag && localTagName(t.name) == "title") inTitle = true;
            else if (t.type == XmlToken::CloseTag && localTagName(t.name) == "title") inTitle = false;
            else if (t.type == XmlToken::Text && inTitle && !fs::trim(t.text).empty()) {
                res.title = fs::trim(t.text);
            }
        }
    }

    auto tokens = tokenizeXml(docXml);

    struct RunState {
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool strike = false;
        std::string color;
        int halfPtSize = 0;
    };

    bool inParagraph = false;
    bool inRun = false;
    bool inText = false;
    bool inTable = false;
    bool paraBidi = false;
    bool paraIsList = false;
    std::string paraStyle;
    std::string paraAlign;
    std::string paraHtml;
    std::string paraPlain;
    std::string activeHyperlink;
    RunState runState;

    auto flushParagraph = [&]() {
        if (!inParagraph) return;
        inParagraph = false;

        std::string trimmedPlain = fs::trim(paraPlain);
        if (!trimmedPlain.empty()) {
            res.paragraphCount++;
            res.plainText += trimmedPlain + "\n";
        }

        if (paraHtml.empty() && !inTable) return;

        bool isRtl = paraBidi || fs::containsArabicText(paraPlain);
        std::string styleLower = fs::toLowerAscii(paraStyle);
        std::string tag = "p";
        if (styleLower.find("title") != std::string::npos && styleLower.find("subtitle") == std::string::npos) tag = "h1";
        else if (styleLower.find("heading1") != std::string::npos || styleLower == "1") tag = "h1";
        else if (styleLower.find("heading2") != std::string::npos || styleLower == "2") tag = "h2";
        else if (styleLower.find("heading3") != std::string::npos || styleLower == "3") tag = "h3";
        else if (styleLower.find("heading4") != std::string::npos || styleLower.find("subtitle") != std::string::npos) tag = "h4";

        std::string attrs = isRtl ? " dir=\"rtl\"" : " dir=\"ltr\"";
        std::string css;
        if (paraAlign == "center") css += "text-align:center;";
        else if (paraAlign == "right" || (paraAlign.empty() && isRtl)) css += "text-align:right;";
        else if (paraAlign == "left") css += "text-align:left;";
        else if (paraAlign == "both" || paraAlign == "distribute") css += "text-align:justify;";

        if (paraIsList) {
            res.bodyHtml += "<div class=\"doc-list-item\"" + attrs;
            if (!css.empty()) res.bodyHtml += " style=\"" + css + "\"";
            res.bodyHtml += ">&#8226; " + (paraHtml.empty() ? std::string("&nbsp;") : paraHtml) + "</div>\n";
        } else {
            res.bodyHtml += "<" + tag + attrs;
            if (!css.empty()) res.bodyHtml += " style=\"" + css + "\"";
            res.bodyHtml += ">" + (paraHtml.empty() ? std::string("&nbsp;") : paraHtml) + "</" + tag + ">\n";
        }

        paraHtml.clear();
        paraPlain.clear();
        paraStyle.clear();
        paraAlign.clear();
        paraBidi = false;
        paraIsList = false;
    };

    for (const auto& tok : tokens) {
        std::string local = localTagName(tok.name);
        bool isOpen = (tok.type == XmlToken::OpenTag || tok.type == XmlToken::SelfCloseTag);
        bool isClose = (tok.type == XmlToken::CloseTag || tok.type == XmlToken::SelfCloseTag);

        if (tok.type == XmlToken::Text) {
            if (inText && inParagraph) {
                paraPlain += tok.text;
                std::string esc = fs::htmlEscape(tok.text);
                std::string prefix, suffix;
                if (!runState.color.empty() || runState.halfPtSize > 0) {
                    std::string rcss;
                    if (!runState.color.empty() && runState.color != "auto") {
                        rcss += "color:#" + fs::htmlEscape(runState.color) + ";";
                    }
                    if (runState.halfPtSize >= 14 && runState.halfPtSize <= 144) {
                        rcss += "font-size:" + std::to_string(runState.halfPtSize / 2) + "pt;";
                    }
                    if (!rcss.empty()) {
                        prefix += "<span style=\"" + rcss + "\">";
                        suffix = "</span>" + suffix;
                    }
                }
                if (runState.bold) { prefix += "<strong>"; suffix = "</strong>" + suffix; }
                if (runState.italic) { prefix += "<em>"; suffix = "</em>" + suffix; }
                if (runState.underline) { prefix += "<u>"; suffix = "</u>" + suffix; }
                if (runState.strike) { prefix += "<s>"; suffix = "</s>" + suffix; }

                paraHtml += prefix + esc + suffix;
            }
            continue;
        }

        if (isOpen) {
            if (local == "tbl") {
                flushParagraph();
                inTable = true;
                res.bodyHtml += "<div class=\"doc-table-wrap\"><table class=\"doc-table\">\n";
            } else if (local == "tr" && inTable) {
                res.bodyHtml += "<tr>";
            } else if (local == "tc" && inTable) {
                res.bodyHtml += "<td>";
            } else if (local == "p") {
                flushParagraph();
                inParagraph = true;
                paraBidi = false;
                paraIsList = false;
                paraStyle.clear();
                paraAlign.clear();
                paraHtml.clear();
                paraPlain.clear();
            } else if (local == "pStyle" && inParagraph) {
                paraStyle = getAttrAnyNs(tok.attrs, "val");
            } else if (local == "bidi" && inParagraph) {
                std::string v = getAttrAnyNs(tok.attrs, "val");
                if (v != "0" && v != "false") paraBidi = true;
            } else if (local == "jc" && inParagraph) {
                paraAlign = getAttrAnyNs(tok.attrs, "val");
            } else if (local == "numPr" && inParagraph) {
                paraIsList = true;
            } else if (local == "r" && inParagraph) {
                inRun = true;
                runState = RunState{};
            } else if (local == "b" && inRun) {
                std::string v = getAttrAnyNs(tok.attrs, "val");
                runState.bold = (v != "0" && v != "false");
            } else if (local == "i" && inRun) {
                std::string v = getAttrAnyNs(tok.attrs, "val");
                runState.italic = (v != "0" && v != "false");
            } else if (local == "u" && inRun) {
                std::string v = getAttrAnyNs(tok.attrs, "val");
                runState.underline = (v != "none" && v != "0" && v != "false");
            } else if (local == "strike" && inRun) {
                std::string v = getAttrAnyNs(tok.attrs, "val");
                runState.strike = (v != "0" && v != "false");
            } else if (local == "color" && inRun) {
                runState.color = getAttrAnyNs(tok.attrs, "val");
            } else if (local == "sz" && inRun) {
                runState.halfPtSize = std::atoi(getAttrAnyNs(tok.attrs, "val").c_str());
            } else if (local == "t" && inParagraph) {
                inText = true;
            } else if (local == "br" && inParagraph) {
                paraHtml += "<br/>";
                paraPlain += "\n";
            } else if (local == "tab" && inParagraph) {
                paraHtml += "&emsp;";
                paraPlain += "\t";
            } else if (local == "hyperlink" && inParagraph) {
                std::string rid = getAttrAnyNs(tok.attrs, "id");
                auto it = rels.find(rid);
                if (it != rels.end() && it->second.isExternal) {
                    activeHyperlink = it->second.target;
                    paraHtml += "<a href=\"" + fs::htmlEscape(activeHyperlink) + "\" target=\"_blank\">";
                }
            } else if ((local == "blip" || local == "imagedata") && inParagraph) {
                std::string rid = getAttrAnyNs(tok.attrs, "embed");
                if (rid.empty()) rid = getAttrAnyNs(tok.attrs, "id");
                auto it = rels.find(rid);
                if (it != rels.end() && !it->second.isExternal) {
                    std::string relPath = it->second.target;
                    while (relPath.compare(0, 3, "../") == 0) relPath.erase(0, 3);
                    if (relPath.compare(0, 5, "word/") != 0) relPath = "word/" + relPath;
                    std::vector<uint8_t> imgBytes;
                    if (zip.extractEntry(relPath, imgBytes) && !imgBytes.empty() && imgBytes.size() <= 8 * 1024 * 1024) {
                        std::string mime = guessImageMime(relPath);
                        std::string b64 = fs::base64Encode(imgBytes.data(), imgBytes.size());
                        paraHtml += "<div class=\"doc-img-box\"><img src=\"data:" + mime + ";base64," + b64 + "\" alt=\"صورة مضمنة\"/></div>";
                    }
                }
            }
        }

        if (isClose) {
            if (local == "t") {
                inText = false;
            } else if (local == "r") {
                inRun = false;
            } else if (local == "hyperlink") {
                if (!activeHyperlink.empty()) {
                    paraHtml += "</a>";
                    activeHyperlink.clear();
                }
            } else if (local == "p") {
                flushParagraph();
            } else if (local == "tc" && inTable) {
                flushParagraph();
                res.bodyHtml += "</td>";
            } else if (local == "tr" && inTable) {
                res.bodyHtml += "</tr>\n";
            } else if (local == "tbl") {
                inTable = false;
                res.bodyHtml += "</table></div>\n";
            }
        }
    }

    flushParagraph();
    res.wordCount = countWordsUtf8(res.plainText);
    res.ok = true;
    return res;
}

}  // namespace litebrowser

